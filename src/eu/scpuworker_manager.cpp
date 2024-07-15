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

#include <climits>
#include <cstring>
#include <sys/stat.h>
#include "eu/scpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "eu/qos_interface.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "eu/co_routine_factory.h"
#include "eu/scpuworker_manager.h"
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
#include "eu/blockaware.h"
#endif
#include "dfx/perf/ffrt_perf.h"

namespace ffrt {
constexpr int MANAGER_DESTRUCT_TIMESOUT = 1000;
SCPUWorkerManager::SCPUWorkerManager()
{
    monitor = new SCPUMonitor({
        [this] (const QoS& qos) { return this->IncWorker(qos); },
        [this] (const QoS& qos) { this->WakeupWorkers(qos); },
        [this] (const QoS& qos) { return this->GetTaskCount(qos); },
        [this] (const QoS& qos) { return this->GetWorkerCount(qos); },
        [this] (const QoS& qos) { return this->GetBlockingNum(qos); },
    });
}

SCPUWorkerManager::~SCPUWorkerManager()
{
    tearDown = true;
    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        int try_cnt = MANAGER_DESTRUCT_TIMESOUT;
        while (try_cnt-- > 0) {
            pollersMtx[qos].unlock();
            PollerProxy::Instance()->GetPoller(qos).WakeUp();
            sleepCtl[qos].cv.notify_all();
            {
                usleep(1000);
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
    delete monitor;
}

WorkerAction SCPUWorkerManager::WorkerIdleAction(const WorkerThread* thread)
{
    if (tearDown) {
        return WorkerAction::RETIRE;
    }

    auto& ctl = sleepCtl[thread->GetQos()];
    std::unique_lock lk(ctl.mutex);
    (void)monitor->IntoSleep(thread->GetQos());
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    BlockawareEnterSleeping();
#endif
    FFRT_PERF_WORKER_IDLE(static_cast<int>(thread->GetQos()));
#if !defined(IDLE_WORKER_DESTRUCT)
    constexpr int waiting_seconds = 10;
#else
    constexpr int waiting_seconds = 5;
#endif
    if (ctl.cv.wait_for(lk, std::chrono::seconds(waiting_seconds), [this, thread] {
        bool taskExistence = GetTaskCount(thread->GetQos()) ||
            reinterpret_cast<const CPUWorker*>(thread)->priority_task ||
            reinterpret_cast<const CPUWorker*>(thread)->localFifo.GetLength();
        bool needPoll = !PollerProxy::Instance()->GetPoller(thread->GetQos()).DetermineEmptyMap() &&
            (polling_[thread->GetQos()] == 0);
        return tearDown || taskExistence || needPoll;
        })) {
        monitor->WakeupCount(thread->GetQos());
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        int err = BlockawareLeaveSleeping();
        if (err != 0) {
            FFRT_LOGE("leave sleeping fail, err[%d]", err);
        }
#endif
        FFRT_PERF_WORKER_AWAKE(static_cast<int>(thread->GetQos()));
        FFRT_LOGD("worker awake");
        return WorkerAction::RETRY;
    } else {
#if !defined(IDLE_WORKER_DESTRUCT)
        monitor->IntoDeepSleep(thread->GetQos());
        CoStackFree();
        if (monitor->IsExceedDeepSleepThreshold()) {
            ffrt::CoRoutineReleaseMem();
        }

        ctl.cv.wait(lk, [this, thread] {
            return tearDown || GetTaskCount(thread->GetQos()) ||
            reinterpret_cast<const CPUWorker*>(thread)->priority_task ||
            reinterpret_cast<const CPUWorker*>(thread)->localFifo.GetLength();
            });

        monitor->OutOfDeepSleep(thread->GetQos());
        FFRT_LOGD("worker awake");
        return WorkerAction::RETRY;
#else
        monitor->TimeoutCount(thread->GetQos());
        FFRT_LOGD("worker exit");
        return WorkerAction::RETIRE;
#endif
    }
}

void SCPUWorkerManager::WorkerPrepare(WorkerThread* thread)
{
    WorkerJoinTg(thread->GetQos(), thread->Id());
}
void SCPUWorkerManager::WakeupWorkers(const QoS& qos)
{
    if (tearDown) {
        return;
    }

    auto& ctl = sleepCtl[qos()];
    ctl.cv.notify_one();
    FFRT_PERF_WORKER_WAKE(static_cast<int>(qos));
}

} // namespace ffrt
