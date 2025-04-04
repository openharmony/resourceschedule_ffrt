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

#include <cstring>
#include <sys/stat.h>
#include "qos.h"
#include "dfx/perf/ffrt_perf.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_strategy.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "eu/qos_interface.h"
#include "eu/cpuworker_manager.h"
#include "util/ffrt_facade.h"
#ifdef FFRT_WORKER_MONITOR
#include "util/ffrt_facade.h"
#endif
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
#include "eu/blockaware.h"
#endif

namespace {
void InsertTask(void* task, int qos)
{
    ffrt_executor_task_t* executorTask = reinterpret_cast<ffrt_executor_task_t*>(task);
    ffrt::LinkedList* node = reinterpret_cast<ffrt::LinkedList*>(&executorTask->wq);
    if (!ffrt::FFRTFacade::GetSchedInstance()->InsertNode(node, qos)) {
        FFRT_LOGE("Insert task failed.");
    }
}
}

namespace ffrt {
bool CPUWorkerManager::IncWorker(const QoS& qos)
{
    QoS localQos = qos;
    int workerQos = localQos();
    if (workerQos < 0 || workerQos >= QoS::MaxNum()) {
        FFRT_LOGE("IncWorker qos:%d is invaild", workerQos);
        return false;
    }
    if (tearDown) {
        FFRT_LOGE("CPU Worker Manager exit");
        return false;
    }

    workerNum.fetch_add(1);
    auto worker = CPUManagerStrategy::CreateCPUWorker(localQos, this);
    auto uniqueWorker = std::unique_ptr<WorkerThread>(worker);
    if (uniqueWorker == nullptr) {
        workerNum.fetch_sub(1);
        FFRT_LOGE("IncWorker failed: worker is nullptr\n");
        return false;
    }

    {
        std::lock_guard<std::shared_mutex> lock(groupCtl[workerQos].tgMutex);
        if (uniqueWorker->Exited()) {
            FFRT_LOGE("IncWorker failed: worker has exited\n");
            return false;
        }

        auto result = groupCtl[workerQos].threads.emplace(worker, std::move(uniqueWorker));
        if (!result.second) {
            FFRT_LOGE("qos:%d worker insert fail:%d", workerQos, result.second);
            return false;
        }
    }
#ifdef FFRT_WORKER_MONITOR
    FFRTFacade::GetWMInstance().SubmitTask();
#endif
    FFRTTraceRecord::UseFfrt();
    return true;
}

int CPUWorkerManager::GetTaskCount(const QoS& qos)
{
    auto& sched = FFRTFacade::GetSchedInstance()->GetScheduler(qos);
    return sched.RQSize();
}

int CPUWorkerManager::GetWorkerCount(const QoS& qos)
{
    std::shared_lock<std::shared_mutex> lck(groupCtl[qos()].tgMutex);
    return groupCtl[qos()].threads.size();
}

unsigned int CPUWorkerManager::StealTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return 0;
    }

    std::shared_lock<std::shared_mutex> lck(groupCtl[thread->GetQos()].tgMutex);
    if (GetStealingWorkers(thread->GetQos()) > groupCtl[thread->GetQos()].threads.size() / 2) {
        return 0;
    }

    AddStealingWorker(thread->GetQos());
    std::unordered_map<WorkerThread*, std::unique_ptr<WorkerThread>>::iterator iter =
        groupCtl[thread->GetQos()].threads.begin();
    while (iter != groupCtl[thread->GetQos()].threads.end()) {
        SpmcQueue* queue = &(reinterpret_cast<CPUWorker*>(iter->first)->localFifo);
        unsigned int queueLen = queue->GetLength();
        if (iter->first != thread && queueLen > 0) {
            unsigned int popLen = queue->PopHeadToAnotherQueue(
                reinterpret_cast<CPUWorker*>(thread)->localFifo, (queueLen + 1) / 2, thread->GetQos(), InsertTask);
            SubStealingWorker(thread->GetQos());
            return popLen;
        }
        iter++;
    }
    SubStealingWorker(thread->GetQos());
    return 0;
}

PollerRet CPUWorkerManager::TryPoll(const WorkerThread* thread, int timeout)
{
    if (tearDown || FFRTFacade::GetPPInstance().GetPoller(thread->GetQos()).DetermineEmptyMap()) {
        return PollerRet::RET_NULL;
    }
    auto& pollerMtx = pollersMtx[thread->GetQos()];
    if (pollerMtx.try_lock()) {
        polling_[thread->GetQos()] = 1;
        if (timeout == -1) {
            monitor->IntoPollWait(thread->GetQos());
        }
        PollerRet ret = FFRTFacade::GetPPInstance().GetPoller(thread->GetQos()).PollOnce(timeout);
        if (timeout == -1) {
            monitor->OutOfPollWait(thread->GetQos());
        }
        polling_[thread->GetQos()] = 0;
        pollerMtx.unlock();
        return ret;
    }
    return PollerRet::RET_NULL;
}

void CPUWorkerManager::NotifyLocalTaskAdded(const QoS& qos)
{
    if (stealWorkers[qos()].load(std::memory_order_relaxed) == 0) {
        monitor->Notify(qos, TaskNotifyType::TASK_LOCAL);
    }
}

void CPUWorkerManager::NotifyTaskPicked(const WorkerThread* thread)
{
    monitor->Notify(thread->GetQos(), TaskNotifyType::TASK_PICKED);
}

void CPUWorkerManager::WorkerRetired(WorkerThread* thread)
{
    pid_t pid = thread->Id();
    int qos = static_cast<int>(thread->GetQos());

    {
        std::unique_lock<std::shared_mutex> lck(groupCtl[qos].tgMutex);
        thread->SetExited(true);
        thread->Detach();
        auto worker = std::move(groupCtl[qos].threads[thread]);
        int ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(qos, pid);
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
    workerNum.fetch_sub(1);
}

void CPUWorkerManager::NotifyTaskAdded(const QoS& qos)
{
    monitor->Notify(qos, TaskNotifyType::TASK_ADDED);
}

void CPUWorkerManager::NotifyWorkers(const QoS& qos, int number)
{
    monitor->NotifyWorkers(qos, number);
}

CPUWorkerManager::CPUWorkerManager()
{
    groupCtl[qos_deadline_request].tg = std::make_unique<ThreadGroup>();
}

void CPUWorkerManager::WorkerJoinTg(const QoS& qos, pid_t pid)
{
    std::shared_lock<std::shared_mutex> lock(groupCtl[qos()].tgMutex);
    if (qos == qos_user_interactive) {
        (void)JoinWG(pid);
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Join(pid);
}

void CPUWorkerManager::WorkerLeaveTg(const QoS& qos, pid_t pid)
{
    if (qos == qos_user_interactive) {
        (void)LeaveWG(pid);
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Leave(pid);
}

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
bool CPUWorkerManager::IsExceedRunningThreshold(const WorkerThread* thread)
{
    return monitor->IsExceedRunningThreshold(thread->GetQos());
}

bool CPUWorkerManager::IsBlockAwareInit()
{
    return monitor->IsBlockAwareInit();
}
#endif
} // namespace ffrt
