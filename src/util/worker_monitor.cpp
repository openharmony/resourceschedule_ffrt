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
#include <dlfcn.h>
#ifdef FFRT_OH_TRACE_ENABLE
#include "backtrace_local.h"
#endif

#include "eu/execute_unit.h"
#include "eu/worker_manager.h"
#include "internal_inc/osal.h"
#include "sched/scheduler.h"

namespace {
constexpr int TASK_OVERRUN_THRESHOLD = 1000;
constexpr uint64_t PROCESS_NAME_BUFFER_LENGTH = 1024;
constexpr uint64_t MONITOR_TIMEOUT_MAX_COUNT = 2;
constexpr uint64_t MONITOR_SAMPLING_CYCLE_US = 500 * 1000;
constexpr uint64_t TIMEOUT_RECORD_CYCLE_US = 60 * 1000 * 1000;
}

namespace ffrt {
WorkerMonitor::WorkerMonitor()
{
    char processName[PROCESS_NAME_BUFFER_LENGTH];
    GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
    // hdc在调用hdc shell的时候会长期占用worker，过滤该进程以防止一直打印超时信息
    // 另外，对hdc进程进行监控会概率性导致hdc断连，原因未知，暂时规避
    skipSampling_ = (strstr(processName, "hdcd") != nullptr);
    SubmitSamplingTask();
}

WorkerMonitor::~WorkerMonitor()
{
    std::lock_guard lock(mutex_);
    skipSampling_ = true;
}

void WorkerMonitor::SubmitSamplingTask()
{
    if (skipSampling_) {
        return;
    }

    waitEntry_.tp = std::chrono::steady_clock::now() + std::chrono::microseconds(MONITOR_SAMPLING_CYCLE_US);
    waitEntry_.cb = ([this](WaitEntry* we) { CheckWorkerStatus(); });
    if (!DelayedWakeup(waitEntry_.tp, &waitEntry_, waitEntry_.cb)) {
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
    QoS _qos = QoS(static_cast<int>(qos_max));
    for (int i = 0; i < _qos() + 1; i++) {
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

            RecordTimeoutFunctionInfo(worker, workerTask);
        }
    }

    SubmitSamplingTask();
}

void WorkerMonitor::RecordTimeoutFunctionInfo(WorkerThread* worker, CPUEUTask* workerTask)
{
    auto workerIter = workerStatus_.find(worker);
    if (workerIter == workerStatus_.end()) {
        workerStatus_[worker] = { workerTask, 0 };
        return;
    }

    if (workerIter->second.first == workerTask) {
        if (++workerIter->second.second >= static_cast<int>(MONITOR_TIMEOUT_MAX_COUNT)) {
            worker->SetWorkerBlocked(true);
            RecordSymbolAndBacktrace(worker->Id());
            workerIter->second.second =
                -static_cast<int>(TIMEOUT_RECORD_CYCLE_US / MONITOR_SAMPLING_CYCLE_US - MONITOR_TIMEOUT_MAX_COUNT);
        }
        return;
    }
    worker->SetWorkerBlocked(false);
    workerIter->second = { workerTask, 0 };
}

void WorkerMonitor::RecordSymbolAndBacktrace(int tid)
{
    FFRT_LOGW("Function in occupies worker for more than 1s.");

#ifdef FFRT_OH_TRACE_ENABLE
    std::string dumpInfo;
    if (OHOS::HiviewDFX::GetBacktraceStringByTid(dumpInfo, tid, 0, false)) {
        FFRT_LOGW("Backtrace:\n%s", dumpInfo.c_str());
    }
#endif
}
}
