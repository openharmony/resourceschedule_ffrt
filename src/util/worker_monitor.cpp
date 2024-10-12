/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "worker_monitor.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#ifdef FFRT_OH_TRACE_ENABLE
#include "backtrace_local.h"
#endif

#include "dfx/sysevent/sysevent.h"
#include "eu/execute_unit.h"
#include "eu/worker_manager.h"
#include "eu/co_routine_factory.h"
#include "internal_inc/osal.h"
#include "sched/scheduler.h"
#include "util/ffrt_facade.h"

namespace {
constexpr int HISYSEVENT_TIMEOUT_SEC = 60;
constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
constexpr int MONITOR_SAMPLING_CYCLE_US = 500 * 1000;
constexpr int SAMPLING_TIMES_PER_SEC = 1000 * 1000 / MONITOR_SAMPLING_CYCLE_US;
constexpr uint64_t TIMEOUT_MEMSHRINK_CYCLE_US = 60 * 1000 * 1000;
constexpr int RECORD_IPC_INFO_TIME_THRESHOLD = 600;
constexpr char IPC_STACK_NAME[] = "libipc_core";
constexpr char TRANSACTION_PATH[] = "/proc/transaction_proc";
constexpr char CONF_FILEPATH[] = "/etc/ffrt/worker_monitor.conf";
const std::vector<int> TIMEOUT_RECORD_CYCLE_LIST = { 1, 3, 5, 10, 30, 60, 10 * 60, 30 * 60 };
}

namespace ffrt {
WorkerMonitor::WorkerMonitor()
{
    // 获取当前进程名称
    char processName[PROCESS_NAME_BUFFER_LENGTH];
    GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);

    // 从配置文件读取黑名单
    std::string skipProcess;
    std::ifstream file(CONF_FILEPATH);
    if (file.is_open()) {
        while (std::getline(file, skipProcess)) {
            if (strstr(processName, skipProcess.c_str()) != nullptr) {
                skipSampling_ = true;
                return;
            }
        }
    } else {
        FFRT_LOGW("worker_monitor.conf does not exist or file permission denied");
    }
}

WorkerMonitor::~WorkerMonitor()
{
    std::lock_guard lock(mutex_);
    skipSampling_ = true;
}

WorkerMonitor& WorkerMonitor::GetInstance()
{
    static WorkerMonitor instance;
    return instance;
}

void WorkerMonitor::SubmitTask()
{
    if (skipSampling_) {
        return;
    }

    std::lock_guard submitTaskLock(submitTaskMutex_);
    if (samplingTaskExit_) {
        SubmitSamplingTask();
        samplingTaskExit_ = false;
    }
    if (memReleaseTaskExit_) {
        SubmitMemReleaseTask();
        memReleaseTaskExit_ = false;
    }
}

void WorkerMonitor::SubmitSamplingTask()
{
    watchdogWaitEntry_.tp = std::chrono::steady_clock::now() + std::chrono::microseconds(MONITOR_SAMPLING_CYCLE_US);
    watchdogWaitEntry_.cb = ([this](WaitEntry* we) { CheckWorkerStatus(); });
    if (!DelayedWakeup(watchdogWaitEntry_.tp, &watchdogWaitEntry_, watchdogWaitEntry_.cb)) {
        FFRT_LOGW("Set delayed worker failed.");
    }
}

void WorkerMonitor::SubmitMemReleaseTask()
{
    if (skipSampling_) {
        return;
    }
    memReleaseWaitEntry_.tp = std::chrono::steady_clock::now() + std::chrono::microseconds(TIMEOUT_MEMSHRINK_CYCLE_US);
    memReleaseWaitEntry_.cb = ([this](WaitEntry* we) {
        std::lock_guard lock(mutex_);
        if (skipSampling_) {
            return;
        }

        WorkerGroupCtl* workerGroup = FFRTFacade::GetEUInstance().GetGroupCtl();
        {
            bool noWorkerThreads = true;
            std::lock_guard submitTaskLock(submitTaskMutex_);
            for (int i = 0; i < QoS::MaxNum(); i++) {
                std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
                if (!workerGroup[i].threads.empty()) {
                    noWorkerThreads = false;
                    break;
                }
            }
            if (noWorkerThreads) {
                CoRoutineReleaseMem();
                samplingTaskExit_ = true;
                return;
            }
        }

        CoRoutineReleaseMem();
        SubmitMemReleaseTask();
    });
    if (!DelayedWakeup(memReleaseWaitEntry_.tp, &memReleaseWaitEntry_, memReleaseWaitEntry_.cb)) {
        FFRT_LOGW("Set delayed worker failed.");
    }
}

void WorkerMonitor::CheckWorkerStatus()
{
    std::lock_guard lock(mutex_);
    if (skipSampling_) {
        return;
    }

    WorkerGroupCtl* workerGroup = FFRTFacade::GetEUInstance().GetGroupCtl();
    {
        bool noWorkerThreads = true;
        std::lock_guard submitTaskLock(submitTaskMutex_);
        for (int i = 0; i < QoS::MaxNum(); i++) {
            std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
            if (!workerGroup[i].threads.empty()) {
                noWorkerThreads = false;
                break;
            }
        }
        if (noWorkerThreads) {
            samplingTaskExit_ = true;
            return;
        }
    }
    std::vector<TimeoutFunctionInfo> timeoutFunctions;
    for (int i = 0; i < QoS::MaxNum(); i++) {
        std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
        size_t coWorkerCount = workerGroup[i].threads.size();
        for (auto& thread : workerGroup[i].threads) {
            WorkerThread* worker = thread.first;
            CPUEUTask* workerTask = worker->curTask;
            if (workerTask == nullptr) {
                workerStatus_.erase(worker);
                continue;
            }

            RecordTimeoutFunctionInfo(coWorkerCount, worker, workerTask, timeoutFunctions);
        }
    }

    for (const auto& timeoutFunction : timeoutFunctions) {
        RecordSymbolAndBacktrace(timeoutFunction);
    }

    SubmitSamplingTask();
}

void WorkerMonitor::RecordTimeoutFunctionInfo(size_t coWorkerCount, WorkerThread* worker, CPUEUTask* workerTask,
    std::vector<TimeoutFunctionInfo>& timeoutFunctions)
{
    auto workerIter = workerStatus_.find(worker);
    if (workerIter == workerStatus_.end()) {
        workerStatus_[worker] = TaskTimeoutInfo(workerTask);
        return;
    }

    TaskTimeoutInfo& taskInfo = workerIter->second;
    if (taskInfo.task_ == workerTask) {
        if (++taskInfo.sampledTimes_ < SAMPLING_TIMES_PER_SEC) {
            return;
        }

        taskInfo.sampledTimes_ = 0;
        if (++taskInfo.executionTime_ % TIMEOUT_RECORD_CYCLE_LIST[taskInfo.recordLevel_] == 0) {
            timeoutFunctions.emplace_back(static_cast<int>(worker->GetQos()), coWorkerCount, worker->Id(),
                taskInfo.executionTime_, worker->curTaskType_, worker->curTaskGid_, worker->curTaskLabel_);
            if (taskInfo.recordLevel_ < static_cast<int>(TIMEOUT_RECORD_CYCLE_LIST.size()) - 1) {
                taskInfo.recordLevel_++;
            }
        }

        return;
    }

    if (taskInfo.executionTime_ > 0) {
        FFRT_LOGI("Tid[%d] function is executed, which occupies worker for [%d]s.",
            worker->Id(), taskInfo.executionTime_);
    }
    workerIter->second = TaskTimeoutInfo(workerTask);
}

void WorkerMonitor::RecordSymbolAndBacktrace(const TimeoutFunctionInfo& timeoutFunction)
{
    std::stringstream ss;
    char processName[PROCESS_NAME_BUFFER_LENGTH];
    GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
    ss << "Task_Sch_Timeout: process name:[" << processName << "], Tid:[" << timeoutFunction.tid_ <<
        "], Worker QoS Level:[" << timeoutFunction.qosLevel_ << "], Concurrent Worker Count:[" <<
        timeoutFunction.coWorkerCount_ << "], Task Type:[" << timeoutFunction.type_ << "], ";
    if (timeoutFunction.type_ == ffrt_normal_task || timeoutFunction.type_ == ffrt_queue_task) {
        ss << "Task Name:[" << timeoutFunction.label_ << "], Task Id:[" << timeoutFunction.gid_ << "], ";
    }
    ss << "occupies worker for more than [" << timeoutFunction.executionTime_ << "]s";
    FFRT_LOGW("%s", ss.str().c_str());

#ifdef FFRT_OH_TRACE_ENABLE
    std::string dumpInfo;
    if (OHOS::HiviewDFX::GetBacktraceStringByTid(dumpInfo, timeoutFunction.tid_, 0, false)) {
        FFRT_LOGW("Backtrace:\n%s", dumpInfo.c_str());
        if (timeoutFunction.executionTime_ >= RECORD_IPC_INFO_TIME_THRESHOLD) {
            RecordIpcInfo(dumpInfo);
        }
    }
#endif
#ifdef FFRT_SEND_EVENT
    if (timeoutFunction.executionTime_ == HISYSEVENT_TIMEOUT_SEC) {
        std::string processNameStr = std::string(processName);
        std::string senarioName = "Task_Sch_Timeout";
        TaskTimeoutReport(ss, processNameStr, senarioName);
    }
#endif
}

void WorkerMonitor::RecordIpcInfo(const std::string& dumpInfo)
{
    if (dumpInfo.find(IPC_STACK_NAME) == std::string::npos) {
        return;
    }

    std::ifstream transactionFile(TRANSACTION_PATH);
    FFRT_COND_DO_ERR(!transactionFile.is_open(), return, "open transaction_proc failed");

    FFRT_LOGW("transaction_proc:");
    std::string line;
    while (getline(transactionFile, line)) {
        if (std::regex_match(line, std::regex(".*to.*code.*"))) {
            FFRT_LOGW("%s", line.c_str());
        }
    }

    transactionFile.close();
}
}
