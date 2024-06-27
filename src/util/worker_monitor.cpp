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
#include <regex>
#ifdef FFRT_OH_TRACE_ENABLE
#include "backtrace_local.h"
#endif

#include "eu/execute_unit.h"
#include "eu/worker_manager.h"
#include "internal_inc/osal.h"
#include "sched/scheduler.h"

namespace {
constexpr int TASK_OVERRUN_THRESHOLD = 1000;
constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
constexpr int MONITOR_SAMPLING_CYCLE_US = 500 * 1000;
constexpr int SAMPLING_TIMES_PER_SEC = 1000 * 1000 / MONITOR_SAMPLING_CYCLE_US;
constexpr int RECORD_TIME_PER_LEVEL = 10;
constexpr int RECORD_IPC_INFO_TIME_THRESHOLD = 600;
constexpr char TRANSACTION_PATH[] = "/proc/transaction_proc";
const std::vector<std::string> SKIP_SAMPLING_PROCESS = {"hdcd", "updater"};
const std::vector<int> TIMEOUT_RECORD_CYCLE_LIST = {
    1000 * 1000, 60 * 1000 * 1000, 10 * 60 * 1000 * 1000, 30 * 60 * 1000 * 1000
};
}

namespace ffrt {
WorkerMonitor::WorkerMonitor()
{
    char processName[PROCESS_NAME_BUFFER_LENGTH];
    GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
    // hdc在调用hdc shell的时候会长期占用worker，过滤该进程以防止一直打印超时信息
    // 另外，对hdc进程进行监控会概率性导致hdc断连，原因未知，暂时规避
    for (const auto& skipProcess : SKIP_SAMPLING_PROCESS) {
        if (strstr(processName, skipProcess.c_str()) != nullptr) {
            skipSampling_ = true;
            break;
        }
    }

    SubmitSamplingTask();
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

    std::lock_guard lock(submitTaskMutex_);
    if (samplingTaskExit_) {
        SubmitSamplingTask();
        samplingTaskExit_ = false;
    }
}

void WorkerMonitor::SubmitSamplingTask()
{
    if (skipSampling_) {
        return;
    }

    watchdogWaitEntry_.tp = std::chrono::steady_clock::now() + std::chrono::microseconds(MONITOR_SAMPLING_CYCLE_US);
    watchdogWaitEntry_.cb = ([this](WaitEntry* we) { CheckWorkerStatus(); });
    if (!DelayedWakeup(watchdogWaitEntry_.tp, &watchdogWaitEntry_, watchdogWaitEntry_.cb)) {
        FFRT_LOGW("Set delayed worker failed.");
    }
}

void WorkerMonitor::CheckWorkerStatus()
{
    std::lock_guard lock(mutex_);
    if (skipSampling_) {
        return;
    }

    WorkerGroupCtl* workerGroup = ExecuteUnit::Instance().GetGroupCtl();
    {
        bool noWorkerThreads = true;
        std::lock_guard lock(submitTaskMutex_);
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

    std::vector<std::pair<int, int>> timeoutFunctions;
    for (int i = 0; i < QoS::MaxNum(); i++) {
        auto& sched = FFRTScheduler::Instance()->GetScheduler(QoS(i));
        int taskCount = sched.RQSize();
        if (taskCount >= TASK_OVERRUN_THRESHOLD) {
            FFRT_LOGW("qos [%d], task count [%d] exceeds threshold.", i, taskCount);
        }

        std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
        for (auto& thread : workerGroup[i].threads) {
            WorkerThread* worker = thread.first;
            CPUEUTask* workerTask = worker->curTask;
            if (workerTask == nullptr) {
                workerStatus_.erase(worker);
                worker->SetWorkerBlocked(false);
                continue;
            }

            RecordTimeoutFunctionInfo(worker, workerTask, timeoutFunctions);
        }
    }

    for (const auto& timeoutFunction : timeoutFunctions) {
        RecordSymbolAndBacktrace(timeoutFunction.first, timeoutFunction.second);
    }

    SubmitSamplingTask();
}

void WorkerMonitor::RecordTimeoutFunctionInfo(WorkerThread* worker, CPUEUTask* workerTask,
    std::vector<std::pair<int, int>>& timeoutFunctions)
{
    auto workerIter = workerStatus_.find(worker);
    if (workerIter == workerStatus_.end()) {
        workerStatus_[worker] = TaskTimeoutInfo(workerTask);
        return;
    }

    TaskTimeoutInfo& taskInfo = workerIter->second;
    if (taskInfo.task_ == workerTask) {
        int taskExecutionTime = ++taskInfo.sampledTimes_ * MONITOR_SAMPLING_CYCLE_US;
        if (taskExecutionTime % TIMEOUT_RECORD_CYCLE_LIST[taskInfo.recordLevel_] == 0) {
            worker->SetWorkerBlocked(true);
            timeoutFunctions.emplace_back(std::make_pair(worker->Id(), taskInfo.sampledTimes_));
            if (taskInfo.recordLevel_ < static_cast<int>(TIMEOUT_RECORD_CYCLE_LIST.size()) - 1 &&
                taskInfo.sampledTimes_ % RECORD_TIME_PER_LEVEL == 0) {
                taskInfo.recordLevel_++;
            }
        }
        return;
    }

    worker->SetWorkerBlocked(false);
    if (taskInfo.sampledTimes_ > 0) {
        FFRT_LOGI("Tid[%d] function is executed, which occupies worker for [%d]s.",
            worker->Id(), taskInfo.sampledTimes_ / SAMPLING_TIMES_PER_SEC + 1);
    }
    workerIter->second = TaskTimeoutInfo(workerTask);
}

void WorkerMonitor::RecordSymbolAndBacktrace(int tid, int sampleTimes)
{
    int sampleSeconds = (sampleTimes == 0) ? 1 : sampleTimes / SAMPLING_TIMES_PER_SEC;
    FFRT_LOGW("Tid[%d] function occupies worker for more than [%d]s.", tid, sampleSeconds);

#ifdef FFRT_OH_TRACE_ENABLE
    std::string dumpInfo;
    if (OHOS::HiviewDFX::GetBacktraceStringByTid(dumpInfo, tid, 0, false)) {
        FFRT_LOGW("Backtrace:\n%s", dumpInfo.c_str());
        if (sampleSeconds >= RECORD_IPC_INFO_TIME_THRESHOLD) {
            RecordIpcInfo(dumpInfo);
        }
    }
#endif
}

void WorkerMonitor::RecordIpcInfo(const std::string& dumpInfo)
{
    if (dumpInfo.find("libipc_core") == std::string::npos) {
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
