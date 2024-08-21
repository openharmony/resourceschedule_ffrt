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
#include "eu/worker_thread.h"
#include "tm/cpu_task.h"

namespace ffrt {
struct TaskTimeoutInfo {
    CPUEUTask* task_ = nullptr;
    int recordLevel_ = 0;
    int sampledTimes_ = -2;

    TaskTimeoutInfo() {}
    explicit TaskTimeoutInfo(CPUEUTask* task) : task_(task) {}
};

struct TimeoutFunctionInfo {
    size_t qosLevel_;
    int coWorkerCount_;
    int tid_;
    int sampledTimes_;
    uintptr_t type_;
    uint64_t gid_;
    std::string label_;

    TimeoutFunctionInfo(size_t qosLevel, int coWorkerCount, int workerId, int sampledTimes,
        uintptr_t workerTaskType, uint64_t taskId, std::string workerTaskLabel)
        : qosLevel_(qosLevel), coWorkerCount_(coWorkerCount), tid_(workerId), sampledTimes_(sampledTimes),
        type_(workerTaskType) {
            if (type_ == ffrt_normal_task || type_ == ffrt_queue_task) {
                gid_ = taskId;
                label_ = workerTaskLabel;
            } else {
                gid_ = UINT64_MAX; //该task type 没有 gid
                label_ = "Unsupport_Task_type"; //该task type 没有 label
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
    void RecordTimeoutFunctionInfo(size_t coWorkerCount, WorkerThread* worker,
        CPUEUTask* workerTask, std::vector<TimeoutFunctionInfo>& timeoutFunctions);
    void RecordSymbolAndBacktrace(const TimeoutFunctionInfo& timeoutFunction);
    void RecordIpcInfo(const std::string& dumpInfo);

private:
    std::mutex mutex_;
    std::mutex submitTaskMutex_;
    bool skipSampling_ = false;
    WaitUntilEntry watchdogWaitEntry_;
    WaitUntilEntry memReleaseWaitEntry_;
    std::map<WorkerThread*, TaskTimeoutInfo> workerStatus_;
    bool samplingTaskExit_ = false;
    bool memReleaseTaskExit_ = false;
};
}
#endif