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
#include "dfx/perf/ffrt_perf.h"
#include "eu/co_routine_factory.h"
#include "eu/cpu_manager_interface.h"
#include "eu/qos_interface.h"
#include "eu/scpu_monitor.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "util/ffrt_facade.h"
#include "util/slab.h"
#include "eu/scpuworker_manager.h"
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
#include "eu/blockaware.h"
#endif

namespace {
#if !defined(IDLE_WORKER_DESTRUCT)
constexpr int waiting_seconds = 10;
#else
constexpr int waiting_seconds = 5;
#endif
}

namespace ffrt {
constexpr int MANAGER_DESTRUCT_TIMESOUT = 1000;
constexpr uint64_t DELAYED_WAKED_UP_TASK_TIME_INTERVAL = 5 * 1000 * 1000;
SCPUWorkerManager::SCPUWorkerManager()
{
    monitor = CPUManagerStrategy::CreateCPUMonitor(this);
}

SCPUWorkerManager::~SCPUWorkerManager()
{
    tearDown = true;
    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        int try_cnt = MANAGER_DESTRUCT_TIMESOUT;
        while (try_cnt-- > 0) {
            pollersMtx[qos].unlock();
            FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
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

void SCPUWorkerManager::WorkerRetiredSimplified(WorkerThread* thread)
{
    pid_t pid = thread->Id();
    int qos = static_cast<int>(thread->GetQos());

    bool isEmptyQosThreads = false;
    {
        std::unique_lock<std::shared_mutex> lck(groupCtl[qos].tgMutex);
        thread->SetExited(true);
        thread->Detach();
        auto worker = std::move(groupCtl[qos].threads[thread]);
        int ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        isEmptyQosThreads = groupCtl[qos].threads.empty();
        WorkerLeaveTg(QoS(qos), pid);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        if (IsBlockAwareInit()) {
            ret = BlockawareUnregister();
            if (ret != 0) {
                FFRT_LOGE("blockaware unregister fail, ret[%d]", ret);
            }
        }
#endif
        worker = nullptr;
    }

    // qos has no worker, start delay worker to monitor task
    if (isEmptyQosThreads) {
        FFRT_LOGI("qos has no worker, start delay worker to monitor task, qos %d", qos);
        AddDelayedTask(qos);
    }
}

void SCPUWorkerManager::AddDelayedTask(int qos)
{
    WaitUntilEntry* we = new (SimpleAllocator<WaitUntilEntry>::allocMem()) WaitUntilEntry();
    we->tp = std::chrono::steady_clock::now() + std::chrono::microseconds(DELAYED_WAKED_UP_TASK_TIME_INTERVAL);
    we->cb = ([this, qos](WaitEntry* we) {
        int taskCount = GetTaskCount(QoS(qos));
        std::unique_lock<std::shared_mutex> lck(groupCtl[qos].tgMutex);
        bool isEmpty = groupCtl[qos].threads.empty();
        lck.unlock();

        if (!isEmpty) {
            FFRT_LOGW("qos[%d] has worker, no need add delayed task", qos);
            return;
        }

        if (taskCount != 0) {
            FFRT_LOGI("notify task, qos %d", qos);
            FFRTFacade::GetEUInstance().NotifyTaskAdded(QoS(qos));
        } else {
            AddDelayedTask(qos);
        }
        SimpleAllocator<WaitUntilEntry>::FreeMem(static_cast<WaitUntilEntry*>(we));
    });

    if (!DelayedWakeup(we->tp, we, we->cb)) {
        SimpleAllocator<WaitUntilEntry>::FreeMem(we);
        FFRT_LOGW("add delyaed task failed, qos %d", qos);
    }
}

WorkerAction SCPUWorkerManager::WorkerIdleAction(const WorkerThread* thread)
{
    if (tearDown) {
        return WorkerAction::RETIRE;
    }

    auto& ctl = sleepCtl[thread->GetQos()];
    std::unique_lock lk(ctl.mutex);
    (void)monitor->IntoSleep(thread->GetQos());
    FFRT_PERF_WORKER_IDLE(static_cast<int>(thread->GetQos()));
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    BlockawareEnterSleeping();
#endif
    if (ctl.cv.wait_for(lk, std::chrono::seconds(waiting_seconds), [this, thread] {
        bool taskExistence = GetTaskCount(thread->GetQos()) ||
            reinterpret_cast<const CPUWorker*>(thread)->priority_task ||
            reinterpret_cast<const CPUWorker*>(thread)->localFifo.GetLength();
        bool needPoll = !FFRTFacade::GetPPInstance().GetPoller(thread->GetQos()).DetermineEmptyMap() &&
            (polling_[thread->GetQos()] == 0);
        return tearDown || taskExistence || needPoll;
        })) {
        monitor->WakeupCount(thread->GetQos());
        FFRT_PERF_WORKER_AWAKE(static_cast<int>(thread->GetQos()));
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        BlockawareLeaveSleeping();
#endif
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
        return WorkerAction::RETRY;
#else
        monitor->TimeoutCount(thread->GetQos());
        FFRT_LOGD("worker exit");
        return WorkerAction::RETIRE;
#endif
    }
}

WorkerAction SCPUWorkerManager::WorkerIdleActionSimplified(const WorkerThread* thread)
{
    if (tearDown) {
        return WorkerAction::RETIRE;
    }

    auto& ctl = sleepCtl[thread->GetQos()];
    std::unique_lock lk(ctl.mutex);
    (void)monitor->IntoSleep(thread->GetQos());
    FFRT_PERF_WORKER_IDLE(static_cast<int>(thread->GetQos()));
    if (ctl.cv.wait_for(lk, std::chrono::seconds(waiting_seconds), [this, thread] {
        bool taskExistence = GetTaskCount(thread->GetQos());
        return tearDown || taskExistence;
        })) {
        monitor->WakeupCount(thread->GetQos());
        FFRT_PERF_WORKER_AWAKE(static_cast<int>(thread->GetQos()));
        return WorkerAction::RETRY;
    } else {
#if !defined(IDLE_WORKER_DESTRUCT)
        monitor->IntoDeepSleep(thread->GetQos());
        CoStackFree();
        if (monitor->IsExceedDeepSleepThreshold()) {
            ffrt::CoRoutineReleaseMem();
        }
        ctl.cv.wait(lk, [this, thread] {return tearDown || GetTaskCount(thread->GetQos());});
        monitor->OutOfDeepSleep(thread->GetQos());
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
        FFRT_LOGE("CPU Worker Manager exit");
        return;
    }

    auto& ctl = sleepCtl[qos()];
    ctl.cv.notify_one();
    FFRT_PERF_WORKER_WAKE(static_cast<int>(qos));
}

WorkerThread* CPUManagerStrategy::CreateCPUWorker(const QoS& qos, void* manager)
{
    constexpr int processNameLen = 32;
    static std::once_flag flag;
    static char processName[processNameLen];
    std::call_once(flag, []() {
        GetProcessName(processName, processNameLen);
    });
    CPUWorkerManager* pIns = reinterpret_cast<CPUWorkerManager*>(manager);
    // default strategy of worker ops
    CpuWorkerOps ops {
        CPUWorker::WorkerLooperDefault,
        [pIns] (WorkerThread* thread) { return pIns->PickUpTaskFromGlobalQueue(thread); },
        [pIns] (const WorkerThread* thread) { pIns->NotifyTaskPicked(thread); },
        [pIns] (const WorkerThread* thread) { return pIns->WorkerIdleAction(thread); },
        [pIns] (WorkerThread* thread) { pIns->WorkerRetired(thread); },
        [pIns] (WorkerThread* thread) { pIns->WorkerPrepare(thread); },
        [pIns] (const WorkerThread* thread, int timeout) { return pIns->TryPoll(thread, timeout); },
        [pIns] (WorkerThread* thread) { return pIns->StealTaskBatch(thread); },
        [pIns] (WorkerThread* thread) { return pIns->PickUpTaskBatch(thread); },
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        [pIns] (const WorkerThread* thread) { return pIns->IsExceedRunningThreshold(thread); },
        [pIns] () { return pIns->IsBlockAwareInit(); },
#endif
    };

    return new (std::nothrow) CPUWorker(qos, std::move(ops));
}

CPUMonitor* CPUManagerStrategy::CreateCPUMonitor(void* manager)
{
    constexpr int processNameLen = 32;
    static std::once_flag flag;
    static char processName[processNameLen];
    std::call_once(flag, []() {
        GetProcessName(processName, processNameLen);
    });
    SCPUWorkerManager* pIns = reinterpret_cast<SCPUWorkerManager*>(manager);
    // default strategy of monitor ops
    CpuMonitorOps ops {
        [pIns] (const QoS& qos) { return pIns->IncWorker(qos); },
        [pIns] (const QoS& qos) { pIns->WakeupWorkers(qos); },
        [pIns] (const QoS& qos) { return pIns->GetTaskCount(qos); },
        [pIns] (const QoS& qos) { return pIns->GetWorkerCount(qos); },
        CPUMonitor::HandleTaskNotifyDefault,
    };

    return new SCPUMonitor(std::move(ops));
}
} // namespace ffrt
