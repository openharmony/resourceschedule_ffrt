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
#ifdef FFRT_IO_TASK_SCHEDULER
#include "queue/queue.h"
#include "sync/poller.h"
#endif

namespace ffrt {
class CPUWorker : public WorkerThread {
public:
    CPUWorker(const QoS& qos, CpuWorkerOps&& ops) : WorkerThread(qos), ops(ops)
    {
#ifdef FFRT_IO_TASK_SCHEDULER
        queue_init(&local_fifo, LOCAL_QUEUE_SIZE);
#endif
        Start(CPUWorker::Dispatch, this);
    }

    CpuWorkerOps ops;
#ifdef FFRT_IO_TASK_SCHEDULER
    void* priority_task = nullptr;
    unsigned int tick = 0;
    struct queue_s local_fifo;
    unsigned int global_interval = 60;
    unsigned int budget = 10;
    void** steal_buffer;
#endif

private:
    static void Dispatch(CPUWorker* worker);
    static void Run(TaskCtx* task);
    static void Run(ffrt_executor_task_t* data, ffrt_qos_t qos);
};
} // namespace ffrt
#endif
