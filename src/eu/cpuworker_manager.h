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
<<<<<<< HEAD
#ifdef FFER_IO_TASK_SCHEDULER
#include "sync/poller.h"
#endif
=======
#include "sync/poller.h"
>>>>>>> 1de74c0 (rust)


namespace ffrt {
constexpr int MANAGER_DESTRUCT_TIMESOUT = 1000000;

struct WorkerSleepCtl {
    std::mutex mutex;
    std::condition_variable cv;
};

class CPUWorkerManager : public WorkerManager {
public:
    CPUWorkerManager();

    ~CPUWorkerManager() override
    {
        tearDown = true;
        for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
            int try_cnt = MANAGER_DESTRUCT_TIMESOUT;
            while (try_cnt--) {
<<<<<<< HEAD
#ifdef FFRT_IO_TASK_SCHEDULER
                pollersMtx[qos].unlock();
                PollerProxy::Instance()->GetPoller(qos).WakeUp();
#endif
=======
                pollerExitFlag[qos].store(true, std::memory_order_relaxed);
                std::atomic_thread_fence(std::memory_order_acq_rel);
                pollersMtx[qos].unlock();
                PollerProxy::Instance()->GetPoller(qos).WakeUp();
>>>>>>> 1de74c0 (rust)
                sleepCtl[qos].cv.notify_all();
                {
                    usleep(1);
                    std::shared_lock<std::shared_mutex> lck(groupCtl[qos].tgMutex);
                    if (groupCtl[qos].threads.empty()) {
                        break;
                    }
                }
            }

            if (try_cnt <= 0) {
                FFRT_LOGE("erase qos[%d] threads failed", qos);
            }
        }
    }

    void NotifyTaskAdded(const QoS& qos) override;
<<<<<<< HEAD
#ifdef FFRT_IO_TASK_SCHEDULER
    void NotifyLocalTaskAdded(const QoS& qos) override;
#endif
=======
    void NotifyLocalTaskAdded(const QoS& qos) override;
>>>>>>> 1de74c0 (rust)

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
private:
    bool WorkerTearDown();
    bool IncWorker(const QoS& qos) override;
    bool DecWorker() override
    {return false;}
    void WakeupWorkers(const QoS& qos);
    int GetTaskCount(const QoS& qos);
    void WorkerRetired(WorkerThread* thread);
    TaskCtx* PickUpTask(WorkerThread* thread);
    void NotifyTaskPicked(const WorkerThread* thread);
    WorkerAction WorkerIdleAction(const WorkerThread* thread);
    void WorkerJoinTg(const QoS& qos, pid_t pid);
    void WorkerLeaveTg(const QoS& qos, pid_t pid);

    CPUMonitor monitor;
    WorkerSleepCtl sleepCtl[QoS::Max()];
    fast_mutex pollersMtx[QoS::Max()];
    std::array<std::atomic<bool>, QoS::Max()> pollersExitFlag {false};
    bool tearDown = false;

#ifdef FFRT_IO_TASK_SCHEDULER
    void WorkerSetup(WorkerThread* thread, const QoS& qos);
    PollerRet TryPoll(const WorkerThread* thread, int timeout = -1);
    unsigned int StealTaskBatch(WorkerThread* thread);
    TaskCtx* PickUpTaskBatch(WorkerThread* thread);
    void TryMoveLocal2Global(WorkerThread* thread);
    fast_mutex pollersMtx[QoS::Max()];
    std::atomic_uint64_t stealWorkers[QoS::Max()] = {0};
#endif
};
} // namespace ffrt
#endif
