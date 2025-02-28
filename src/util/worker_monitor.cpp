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
#include "dfx/bbox/bbox.h"

namespace {
constexpr int HISYSEVENT_TIMEOUT_SEC = 60;
constexpr int MONITOR_SAMPLING_CYCLE_US = 500 * 1000;
constexpr unsigned int RECORD_POLLER_INFO_FREQ = 120;
constexpr int SAMPLING_TIMES_PER_SEC = 1000 * 1000 / MONITOR_SAMPLING_CYCLE_US;
constexpr uint64_t TIMEOUT_MEMSHRINK_CYCLE_US = 60 * 1000 * 1000;
constexpr int RECORD_IPC_INFO_TIME_THRESHOLD = 600;
constexpr int BACKTRACE_TASK_QOS = 7;
constexpr char IPC_STACK_NAME[] = "libipc_common";
constexpr char TRANSACTION_PATH[] = "/proc/transaction_proc";
constexpr char CONF_FILEPATH[] = "/etc/ffrt/worker_monitor.conf";
const std::vector<int> TIMEOUT_RECORD_CYCLE_LIST = { 1, 3, 5, 10, 30, 60, 10 * 60, 30 * 60 };
unsigned int g_samplingTaskCount = 0;
}

namespace ffrt {
WorkerMonitor::WorkerMonitor()
{
    // 获取当前进程名称
    const char* processName = GetCurrentProcessName();
    if (strlen(processName) == 0) {
        FFRT_LOGW("Get process name failed, skip worker monitor.");
        skipSampling_ = true;
        return;
    }

    // 从配置文件读取黑名单比对
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

    watchdogWaitEntry_.cb = ([this](WaitEntry* we) { CheckWorkerStatus(); });
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
                memReleaseTaskExit_ = true;
                return;
            }
        }

        CoRoutineReleaseMem();
        SubmitMemReleaseTask();
    });
}

WorkerMonitor::~WorkerMonitor()
{
    FFRT_LOGW("WorkerMonitor destruction enter");
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

    if (g_samplingTaskCount++ % RECORD_POLLER_INFO_FREQ == 0) {
        RecordPollerInfo();
    }

    std::vector<TimeoutFunctionInfo> timeoutFunctions;
    for (int i = 0; i < QoS::MaxNum(); i++) {
        int executionNum = FFRTFacade::GetEUInstance().GetCPUMonitor()->WakedWorkerNum(i);
        int sleepingWorkerNum = FFRTFacade::GetEUInstance().GetCPUMonitor()->SleepingWorkerNum(i);

        std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
        CoWorkerInfo coWorkerInfo(i, workerGroup[i].threads.size(), executionNum, sleepingWorkerNum);
        for (auto& thread : workerGroup[i].threads) {
            WorkerThread* worker = thread.first;
            CPUEUTask* workerTask = static_cast<CPUEUTask*>(worker->curTask);
            if (workerTask == nullptr) {
                workerStatus_.erase(worker);
                continue;
            }

            RecordTimeoutFunctionInfo(coWorkerInfo, worker, workerTask, timeoutFunctions);
        }
    }

    if (timeoutFunctions.size() > 0) {
        FFRTFacade::GetDWInstance().SubmitAsyncTask([this, timeoutFunctions] {
            for (const auto& timeoutFunction : timeoutFunctions) {
                RecordSymbolAndBacktrace(timeoutFunction);
            }
        });
    }

    SubmitSamplingTask();
}

void WorkerMonitor::RecordTimeoutFunctionInfo(const CoWorkerInfo& coWorkerInfo, WorkerThread* worker,
    CPUEUTask* workerTask, std::vector<TimeoutFunctionInfo>& timeoutFunctions)
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
            WorkerInfo workerInfo(worker->Id(), worker->curTaskGid_, worker->curTaskType_, worker->curTaskLabel_);
            timeoutFunctions.emplace_back(coWorkerInfo, workerInfo, taskInfo.executionTime_);
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
    std::string processNameStr = std::string(GetCurrentProcessName());
    ss << "Task_Sch_Timeout: process name:[" << processNameStr << "], Tid:[" << timeoutFunction.workerInfo_.tid_ <<
        "], Worker QoS Level:[" << timeoutFunction.coWorkerInfo_.qosLevel_ << "], Concurrent Worker Count:[" <<
        timeoutFunction.coWorkerInfo_.coWorkerCount_ << "], Execution Worker Number:[" <<
        timeoutFunction.coWorkerInfo_.executionNum_ << "], Sleeping Worker Number:[" <<
        timeoutFunction.coWorkerInfo_.sleepingWorkerNum_ << "], Task Type:[" <<
        timeoutFunction.workerInfo_.workerTaskType_ << "], ";

#ifdef WORKER_CACHE_TASKNAMEID
    if (timeoutFunction.workerInfo_.workerTaskType_ == ffrt_normal_task ||
        timeoutFunction.workerInfo_.workerTaskType_ == ffrt_queue_task) {
        ss << "Task Name:[" << timeoutFunction.workerInfo_.label_ <<
            "], Task Id:[" << timeoutFunction.workerInfo_.gid_ << "], ";
    }
#endif

    ss << "occupies worker for more than [" << timeoutFunction.executionTime_ << "]s";
    FFRT_LOGW("%s", ss.str().c_str());

#ifdef FFRT_OH_TRACE_ENABLE
    std::string dumpInfo;
    if (OHOS::HiviewDFX::GetBacktraceStringByTid(dumpInfo, timeoutFunction.workerInfo_.tid_, 0, false)) {
        FFRT_LOGW("Backtrace:\n%s", dumpInfo.c_str());
        if (timeoutFunction.executionTime_ >= RECORD_IPC_INFO_TIME_THRESHOLD) {
            RecordIpcInfo(dumpInfo, timeoutFunction.workerInfo_.tid_);
        }
    }
#endif
#ifdef FFRT_SEND_EVENT
    if (timeoutFunction.executionTime_ == HISYSEVENT_TIMEOUT_SEC) {
        std::string senarioName = "Task_Sch_Timeout";
        TaskTimeoutReport(ss, processNameStr, senarioName);
    }
#endif
}

void WorkerMonitor::RecordIpcInfo(const std::string& dumpInfo, int tid)
{
    if (dumpInfo.find(IPC_STACK_NAME) == std::string::npos) {
        return;
    }

    std::ifstream transactionFile(TRANSACTION_PATH);
    FFRT_COND_DO_ERR(!transactionFile.is_open(), return, "open transaction_proc failed");

    FFRT_LOGW("transaction_proc:");
    std::string line;
    std::string regexStr = ".*" + std::to_string(tid) + ".*to.*code.*";
    while (getline(transactionFile, line)) {
        if (std::regex_match(line, std::regex(regexStr))) {
            FFRT_LOGW("%s", line.c_str());
        }
    }

    transactionFile.close();
}

void WorkerMonitor::RecordKeyInfo(const std::string& dumpInfo)
{
    if (dumpInfo.find(IPC_STACK_NAME) == std::string::npos || dumpInfo.find("libpower") == std::string::npos) {
        return;
    }

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    std::string keyInfo = SaveKeyInfo();
    FFRT_LOGW("%s", keyInfo.c_str());
#endif
}

void WorkerMonitor::RecordPollerInfo()
{
    std::stringstream ss;
    for (int qos = 0; qos < QoS::MaxNum(); qos++) {
        uint64_t pollCount = FFRTFacade::GetPPInstance().GetPoller(qos).GetPollCount();
        if (pollCount > 0) {
            ss << qos << ":" << pollCount << ";";
        }
    }

    std::string result = ss.str();
    if (!result.empty()) {
        FFRT_LOGW("%s", result.c_str());
    }
}
}
