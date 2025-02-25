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

#ifndef FFRT_WORKER_MANAGER_HPP
#define FFRT_WORKER_MANAGER_HPP

#include <thread>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <unordered_map>

#include "eu/worker_thread.h"
#include "eu/thread_group.h"
#include "eu/cpu_monitor.h"
#include "sync/sync.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
struct WorkerGroupCtl {
    std::unique_ptr<ThreadGroup> tg;
    uint64_t tgRefCount = 0;
    mutable std::shared_mutex tgMutex;
    size_t workerStackSize = 0;
    std::unordered_map<WorkerThread*, std::unique_ptr<WorkerThread>> threads;
};

class WorkerManager {
public:
    uint64_t GetWorkerNum()
    {
        return workerNum.load();
    }

    virtual ~WorkerManager()
    {
    };

    ThreadGroup* JoinTG(QoS& qos);
    void LeaveTG(QoS& qos);
    void JoinRtg(QoS& qos);

    virtual bool IncWorker(const QoS& qos) = 0;
    virtual bool DecWorker() = 0;
    virtual void NotifyTaskAdded(const QoS& qos) = 0;
    virtual void NotifyLocalTaskAdded(const QoS& qos) = 0;
    virtual void NotifyWorkers(const QoS& qos, int number) = 0;
    virtual std::mutex* GetSleepCtl(int qos) = 0;
    virtual CPUMonitor* GetCPUMonitor() = 0;

    WorkerGroupCtl* GetGroupCtl()
    {
        return groupCtl;
    }
protected:
    ThreadGroup tg;
    WorkerGroupCtl groupCtl[QoS::MaxNum()];
    std::atomic_uint64_t workerNum = 0;
};
} // namespace ffrt
#endif
