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

namespace ffrt {
constexpr int MANAGER_DESTRUCT_TIMESOUT = 1000000;
SCPUWorkerManager::SCPUWorkerManager()
{
    monitor = new SCPUMonitor({
        std::bind(&SCPUWorkerManager::IncWorker, this, std::placeholders::_1),
        std::bind(&SCPUWorkerManager::WakeupWorkers, this, std::placeholders::_1),
        std::bind(&SCPUWorkerManager::GetTaskCount, this, std::placeholders::_1)});
}

SCPUWorkerManager::~SCPUWorkerManager()
{
    tearDown = true;
    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        int try_cnt = MANAGER_DESTRUCT_TIMESOUT;
        while (try_cnt-- > 0) {
#ifdef FFRT_IO_TASK_SCHEDULER
            pollersMtx[qos].unlock();
            PollerProxy::Instance()->GetPoller(qos).WakeUp();
#endif
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
#if !defined(IDLE_WORKER_DESTRUCT)
    constexpr int waiting_seconds = 10;
#else
    constexpr int waiting_seconds = 5;
#endif
#ifdef FFRT_IO_TASK_SCHEDULER
    if (ctl.cv.wait_for(lk, std::chrono::seconds(waiting_seconds), [this, thread] {
        return tearDown || GetTaskCount(thread->GetQos()) || ((CPUWorker *)thread)->priority_task ||
        queue_length(&(((CPUWorker *)thread)->local_fifo));
        })) {
#else
    if (ctl.cv.wait_for(lk, std::chrono::seconds(waiting_seconds),
        [this, thread] {return tearDown || GetTaskCount(thread->GetQos());})) {
#endif
        monitor->WakeupCount(thread->GetQos());
        FFRT_LOGD("worker awake");
        return WorkerAction::RETRY;
    } else {
#if !defined(IDLE_WORKER_DESTRUCT)
        monitor->IntoDeepSleep(thread->GetQos());
        CoStackFree();
        if (monitor->IsExceedDeepSleepThreshold()) {
            ffrt::CoRoutineReleaseMem();
        }
#ifdef FFRT_IO_TASK_SCHEDULER
        ctl.cv.wait(lk, [this, thread] {
            return tearDown || GetTaskCount(thread->GetQos()) || ((CPUWorker *)thread)->priority_task ||
            queue_length(&(((CPUWorker *)thread)->local_fifo));
            });
#else
        ctl.cv.wait(lk, [this, thread] {return tearDown || GetTaskCount(thread->GetQos());});
#endif
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
}
} // namespace ffrt
