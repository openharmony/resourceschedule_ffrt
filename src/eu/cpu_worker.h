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
#include "eu/cpu_manager_strategy.h"
#include "c/executor_task.h"
#include "sync/poller.h"
#include "util/spmc_queue.h"
#include "tm/task_base.h"

namespace ffrt {
const unsigned int LOCAL_QUEUE_SIZE = 128;
const unsigned int STEAL_BUFFER_SIZE = LOCAL_QUEUE_SIZE / 2;

class CPUWorker : public WorkerThread {
public:
    CPUWorker(const QoS& qos, CpuWorkerOps&& ops, void* worker_mgr) : WorkerThread(qos), ops(ops)
    {
#ifdef FFRT_SEND_EVENT
        uint64_t freq = 1000000;
#if defined(__aarch64__)
        asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
#endif
        this->cacheFreq = freq;
        this->cacheQos = static_cast<int>(qos);
#endif
        this->worker_mgr = worker_mgr;
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
    static void WorkerLooperDefault(CPUWorker* worker);

private:
    static void* WrapDispatch(void* worker);
    static void Dispatch(CPUWorker* worker);
    static void RunTask(TaskBase* task, CPUWorker* worker);
    static void RunTaskLifo(TaskBase* task, CPUWorker* worker);
    static void* GetTask(CPUWorker* worker);
    static PollerRet TryPoll(CPUWorker* worker, int timeout);
    static bool LocalEmpty(CPUWorker* worker);
#ifdef FFRT_SEND_EVENT
    int cacheQos; // cache int qos
    std::string cacheLabel; // cache string label
    uint64_t cacheFreq = 1000000; // cache cpu freq
#endif
};
} // namespace ffrt
#endif
