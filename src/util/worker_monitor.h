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

#ifndef FFRT_WORKER_MONITOR_H
#define FFRT_WORKER_MONITOR_H

#include <mutex>
#include <map>
#include "eu/worker_thread.h"
#include "tm/cpu_task.h"

namespace ffrt {
struct TaskTimeoutInfo {
    CPUEUTask* task_ = nullptr;
    int recordLevel_ = 0;
    int sampledTimes_ = 0;
    int executionTime_ = 0;

    TaskTimeoutInfo() {}
    explicit TaskTimeoutInfo(CPUEUTask* task) : task_(task) {}
};

struct CoWorkerInfo {
    size_t qosLevel_;
    int coWorkerCount_;
    int executionNum_;
    int sleepingWorkerNum_;

    CoWorkerInfo(size_t qosLevel, int coWorkerCount, int executionNum, int sleepingWorkerNum)
        : qosLevel_(qosLevel), coWorkerCount_(coWorkerCount),
        executionNum_(executionNum), sleepingWorkerNum_(sleepingWorkerNum) {}
};

struct WorkerInfo {
    int tid_;
    uint64_t gid_;
    uintptr_t workerTaskType_;
    std::string label_;

    WorkerInfo(int workerId, uint64_t taskId, uintptr_t workerTaskType, std::string workerTaskLabel)
        : tid_(workerId), gid_(taskId), workerTaskType_(workerTaskType), label_(workerTaskLabel) {}
};

struct TimeoutFunctionInfo {
    CoWorkerInfo coWorkerInfo_;
    WorkerInfo workerInfo_;
    int executionTime_;

    TimeoutFunctionInfo(const CoWorkerInfo& coWorkerInfo, const WorkerInfo& workerInfo, int executionTime)
        : coWorkerInfo_(coWorkerInfo), workerInfo_(workerInfo), executionTime_(executionTime)
    {
        if (workerInfo_.workerTaskType_ != ffrt_normal_task && workerInfo_.workerTaskType_ != ffrt_queue_task) {
            workerInfo_.gid_ = UINT64_MAX; //该task type 没有 gid
            workerInfo_.label_ = "Unsupport_Task_type"; //该task type 没有 label
        }
    }
};

class WorkerMonitor {
public:
    static WorkerMonitor &GetInstance();
    void SubmitTask();

private:
    WorkerMonitor();
    ~WorkerMonitor();
    WorkerMonitor(const WorkerMonitor &) = delete;
    WorkerMonitor(WorkerMonitor &&) = delete;
    WorkerMonitor &operator=(const WorkerMonitor &) = delete;
    WorkerMonitor &operator=(WorkerMonitor &&) = delete;
    void SubmitSamplingTask();
    void SubmitMemReleaseTask();
    void CheckWorkerStatus();
    void RecordTimeoutFunctionInfo(const CoWorkerInfo& coWorkerInfo, WorkerThread* worker,
        CPUEUTask* workerTask, std::vector<TimeoutFunctionInfo>& timeoutFunctions);
    void RecordSymbolAndBacktrace(const TimeoutFunctionInfo& timeoutFunction);
    void RecordIpcInfo(const std::string& dumpInfo, int tid);

private:
    std::mutex mutex_;
    std::mutex submitTaskMutex_;
    bool skipSampling_ = false;
    WaitUntilEntry watchdogWaitEntry_;
    WaitUntilEntry memReleaseWaitEntry_;
    std::map<WorkerThread*, TaskTimeoutInfo> workerStatus_;
    bool samplingTaskExit_ = true;
    bool memReleaseTaskExit_ = true;
};
}
#endif