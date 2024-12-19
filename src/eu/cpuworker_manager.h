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

#ifndef FFRT_CPUWORKER_MANAGER_HPP
#define FFRT_CPUWORKER_MANAGER_HPP

#include "eu/worker_manager.h"
#include "eu/cpu_worker.h"
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_strategy.h"
#include "sync/poller.h"
#include "util/spmc_queue.h"
#include "tm/cpu_task.h"

namespace ffrt {
struct WorkerSleepCtl {
    std::mutex mutex;
    std::condition_variable cv;
};

class CPUWorkerManager : public WorkerManager {
public:
    CPUWorkerManager();

    ~CPUWorkerManager() override
    {
    }

    void NotifyTaskAdded(const QoS& qos) override;
    void NotifyLocalTaskAdded(const QoS& qos) override;
    void NotifyWorkers(const QoS& qos, int number) override;

    std::mutex* GetSleepCtl(int qos) override
    {
        return &sleepCtl[qos].mutex;
    }

    void AddStealingWorker(const QoS& qos)
    {
        stealWorkers[qos].fetch_add(1);
    }

    void SubStealingWorker(const QoS& qos)
    {
        while (1) {
            uint64_t stealWorkersNum = stealWorkers[qos].load();
            if (stealWorkersNum == 0) {
                return;
            }
            if (atomic_compare_exchange_weak(&stealWorkers[qos], &stealWorkersNum, stealWorkersNum - 1)) return;
        }
    }

    uint64_t GetStealingWorkers(const QoS& qos)
    {
        return stealWorkers[qos].load(std::memory_order_relaxed);
    }
    CPUMonitor* GetCPUMonitor() override
    {
        return monitor;
    }

    virtual void WorkerPrepare(WorkerThread* thread) = 0;
    virtual void WakeupWorkers(const QoS& qos) = 0;
    bool IncWorker(const QoS& qos) override;
    int GetTaskCount(const QoS& qos);
    int GetWorkerCount(const QoS& qos);
    void WorkerJoinTg(const QoS& qos, pid_t pid);

    CPUMonitor* monitor = nullptr;
    bool tearDown = false;
    WorkerSleepCtl sleepCtl[QoS::MaxNum()];
    void WorkerLeaveTg(const QoS& qos, pid_t pid);
    uint8_t polling_[QoS::MaxNum()] = {0};
    fast_mutex pollersMtx[QoS::MaxNum()];
    void WorkerRetired(WorkerThread* thread);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    bool IsExceedRunningThreshold(const WorkerThread* thread);
    bool IsBlockAwareInit(void);
#endif

    bool WorkerTearDown();
    bool DecWorker() override
    {return false;}
    virtual void WorkerRetiredSimplified(WorkerThread* thread) = 0;
    void NotifyTaskPicked(const WorkerThread* thread);
    /* strategy options for task pick up */
    CPUEUTask* PickUpTaskFromGlobalQueue(WorkerThread* thread);
    CPUEUTask* PickUpTaskFromLocalQueue(WorkerThread* thread);

    /* strategy options for worker wait action */
    virtual WorkerAction WorkerIdleAction(const WorkerThread* thread) = 0;

    void WorkerSetup(WorkerThread* thread);
    PollerRet TryPoll(const WorkerThread* thread, int timeout = -1);
    unsigned int StealTaskBatch(WorkerThread* thread);
    CPUEUTask* PickUpTaskBatch(WorkerThread* thread);
    std::atomic_uint64_t stealWorkers[QoS::MaxNum()] = {0};
    friend class CPUManagerStrategy;
};
} // namespace ffrt
#endif
