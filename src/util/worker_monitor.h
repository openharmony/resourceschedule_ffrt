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

class WorkerMonitor {
public:
    WorkerMonitor();
    ~WorkerMonitor();

private:
    void SubmitSamplingTask();
    void CheckWorkerStatus();
    void RecordTimeoutFunctionInfo(WorkerThread* worker, CPUEUTask* workerTask,
        std::vector<std::pair<int, int>>& timeoutFunctions);
    void RecordSymbolAndBacktrace(int tid, int sampleTimes);
    void RecordIpcInfo(const std::string& dumpInfo, std::string& ipcInfo);

private:
    std::mutex mutex_;
    bool skipSampling_ = false;
    WaitUntilEntry watchdogWaitEntry_;
    std::map<WorkerThread*, TaskTimeoutInfo> workerStatus_;
};
}
#endif