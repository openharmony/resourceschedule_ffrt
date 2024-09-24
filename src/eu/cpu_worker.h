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

#ifndef FFRT_CPU_WORKER_HPP
#define FFRT_CPU_WORKER_HPP

#include "eu/worker_thread.h"
#include "eu/cpu_manager_interface.h"
#include "c/executor_task.h"
#include "sync/poller.h"
#include "util/spmc_queue.h"
#include "tm/cpu_task.h"

namespace ffrt {
const unsigned int LOCAL_QUEUE_SIZE = 128;
const unsigned int STEAL_BUFFER_SIZE = LOCAL_QUEUE_SIZE / 2;
class CPUWorker : public WorkerThread {
public:
    CPUWorker(const QoS& qos, CpuWorkerOps&& ops) : WorkerThread(qos), ops(ops)
    {
        localFifo.Init(LOCAL_QUEUE_SIZE);
#ifdef FFRT_PTHREAD_ENABLE
        Start(CPUWorker::WrapDispatch, this);
#else
        Start(CPUWorker::Dispatch, this);
#endif
    }

    CpuWorkerOps ops;
    SpmcQueue localFifo;
    void* priority_task = nullptr;
    unsigned int tick = 0;
    unsigned int global_interval = 60;
    unsigned int budget = 10;

public:
    /* strategy options for worklooper function */
    static void WorkerLooperDefault(WorkerThread* p);

private:
    static void* WrapDispatch(void* worker);
    static void Dispatch(CPUWorker* worker);
    static void Run(CPUEUTask* task, CPUWorker* worker);
    static void Run(ffrt_executor_task_t* task, ffrt_qos_t qos);
    static void RunTask(ffrt_executor_task_t* curtask, CPUWorker* worker);
    static void RunTaskLifo(ffrt_executor_task_t* task, CPUWorker* worker);
    static void* GetTask(CPUWorker* worker);
    static PollerRet TryPoll(CPUWorker* worker, int timeout);
    static bool LocalEmpty(CPUWorker* worker);
};
} // namespace ffrt
#endif
