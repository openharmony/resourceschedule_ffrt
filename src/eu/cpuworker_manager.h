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
#include "eu/cpu_manager_interface.h"
#ifdef FFRT_IO_TASK_SCHEDULER
#include "sync/poller.h"
#include "util/spmc_queue.h"
#endif
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
#ifdef FFRT_IO_TASK_SCHEDULER
    void NotifyLocalTaskAdded(const QoS& qos) override;
#endif

    std::mutex* GetSleepCtl(int qos) override
    {
        return &sleepCtl[qos].mutex;
    }

#ifdef FFRT_IO_TASK_SCHEDULER
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
#endif
    CPUMonitor* GetCPUMonitor() override
    {
        return monitor;
    }

    void UpdateBlockingNum(const QoS& qos, bool var)
    {
        if (var) {
            blockingNum[qos]++;
        } else {
            blockingNum[qos]--;
        }
        FFRT_LOGW("QoS %ld blocking num %ld", (int)qos, blockingNum[qos].load());
    }

    int GetBlockingNum(const QoS& qos)
    {
        return blockingNum[qos].load();
    }

protected:
    virtual void WorkerPrepare(WorkerThread* thread) = 0;
    virtual void WakeupWorkers(const QoS& qos) = 0;
    bool IncWorker(const QoS& qos) override;
    int GetTaskCount(const QoS& qos);
    int GetWorkerCount(const QoS& qos);
    void WorkerJoinTg(const QoS& qos, pid_t pid);

    CPUMonitor* monitor = nullptr;
    bool tearDown = false;
    WorkerSleepCtl sleepCtl[QoS::MaxNum()];
#ifdef FFRT_IO_TASK_SCHEDULER
    uint8_t polling_[QoS::MaxNum()] = {};
    fast_mutex pollersMtx[QoS::MaxNum()];
#endif

private:
    bool WorkerTearDown();
    bool DecWorker() override
    {return false;}
    void WorkerRetired(WorkerThread* thread);
    CPUEUTask* PickUpTask(WorkerThread* thread);
    void NotifyTaskPicked(const WorkerThread* thread);
    virtual WorkerAction WorkerIdleAction(const WorkerThread* thread) = 0;
    void WorkerLeaveTg(const QoS& qos, pid_t pid);

#ifdef FFRT_IO_TASK_SCHEDULER
    void WorkerSetup(WorkerThread* thread);
    PollerRet TryPoll(const WorkerThread* thread, int timeout = -1);
    unsigned int StealTaskBatch(WorkerThread* thread);
    CPUEUTask* PickUpTaskBatch(WorkerThread* thread);
    void TryMoveLocal2Global(WorkerThread* thread);
    std::atomic_uint64_t stealWorkers[QoS::MaxNum()] = {0};
#endif
    std::atomic_int blockingNum[QoS::MaxNum()] = {0};
};
} // namespace ffrt
#endif
