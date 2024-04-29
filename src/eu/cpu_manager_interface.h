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

#ifndef FFRT_CPU_MANAGER_INTERFACE_HPP
#define FFRT_CPU_MANAGER_INTERFACE_HPP

#include "eu/worker_thread.h"
#include "qos.h"
#ifdef FFRT_IO_TASK_SCHEDULER
#include "sync/poller.h"
#endif
#include "tm/cpu_task.h"

namespace ffrt {
enum class WorkerAction {
    RETRY = 0,
    RETIRE,
    MAX,
};

enum class TaskNotifyType {
    TASK_PICKED = 0,
    TASK_ADDED,
#ifdef FFRT_IO_TASK_SCHEDULER
    TASK_LOCAL,
#endif
};

enum class SleepType {
    SLEEP_UNTIL_WAKEUP = 0,
    SLEEP_UNTIL_INTERRUPT,
};

struct CpuWorkerOps {
    std::function<CPUEUTask* (WorkerThread*)> PickUpTask;
    std::function<void (const WorkerThread*)> NotifyTaskPicked;
    std::function<WorkerAction (const WorkerThread*)> WaitForNewAction;
    std::function<void (WorkerThread*)> WorkerRetired;
    std::function<void (WorkerThread*)> WorkerPrepare;
#ifdef FFRT_IO_TASK_SCHEDULER
    std::function<PollerRet (const WorkerThread*, int timeout)> TryPoll;
    std::function<unsigned int (WorkerThread*)> StealTaskBatch;
    std::function<CPUEUTask* (WorkerThread*)> PickUpTaskBatch;
    std::function<void (WorkerThread*)> TryMoveLocal2Global;
#endif
    std::function<void (const QoS& qos, bool var)> UpdateBlockingNum;
};

struct CpuMonitorOps {
    std::function<bool (const QoS& qos)> IncWorker;
    std::function<void (const QoS& qos)> WakeupWorkers;
    std::function<int (const QoS& qos)> GetTaskCount;
    std::function<int (const QoS& qos)> GetWorkerCount;
    std::function<int (const QoS& qos)> GetBlockingNum;
};
}
#endif
