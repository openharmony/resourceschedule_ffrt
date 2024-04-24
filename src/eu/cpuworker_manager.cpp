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
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "eu/qos_interface.h"
#include "eu/cpuworker_manager.h"
#include "qos.h"

namespace ffrt {
bool CPUWorkerManager::IncWorker(const QoS& qos)
{
    std::unique_lock<std::shared_mutex> lock(groupCtl[qos()].tgMutex);
    if (tearDown) {
        return false;
    }

    auto worker = std::unique_ptr<WorkerThread>(new (std::nothrow) CPUWorker(qos, {
        std::bind(&CPUWorkerManager::PickUpTask, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::NotifyTaskPicked, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerIdleAction, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerRetired, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerPrepare, this, std::placeholders::_1),
#ifdef FFRT_IO_TASK_SCHEDULER
        std::bind(&CPUWorkerManager::TryPoll, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&CPUWorkerManager::StealTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::PickUpTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::TryMoveLocal2Global, this, std::placeholders::_1),
#endif
        std::bind(&CPUWorkerManager::UpdateBlockingNum, this, std::placeholders::_1, std::placeholders::_2),
    }));
    if (worker == nullptr || worker->Exited()) {
        FFRT_LOGE("Inc CPUWorker: create worker\n");
        return false;
    }
    worker->WorkerSetup(worker.get());
    groupCtl[qos()].threads[worker.get()] = std::move(worker);
    return true;
}

int CPUWorkerManager::GetTaskCount(const QoS& qos)
{
    auto& sched = FFRTScheduler::Instance()->GetScheduler(qos);
    return sched.RQSize();
}

int CPUWorkerManager::GetWorkerCount(const QoS& qos)
{
    return groupCtl[qos()].threads.size();
}

CPUEUTask* CPUWorkerManager::PickUpTask(WorkerThread* thread)
{
    if (tearDown || monitor->IsExceedMaxConcurrency(thread->GetQos())) {
        return nullptr;
    }

    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    return sched.PickNextTask();
}

#ifdef FFRT_IO_TASK_SCHEDULER
CPUEUTask* CPUWorkerManager::PickUpTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }

    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    CPUEUTask* task = sched.PickNextTask();
    if (task == nullptr) {
        return nullptr;
    }

    int wakedWorkerNum = monitor->WakedWorkerNum(thread->GetQos());
    // when there is only one worker, the global queue is equivalent to the local queue
    // prevents local queue tasks that cannot be executed due to blocking tasks
    if (wakedWorkerNum <= 1) {
        return task;
    }

    SpmcQueue* queue = &(reinterpret_cast<CPUWorker*>(thread)->localFifo);
    int expectedTask = GetTaskCount(thread->GetQos()) / wakedWorkerNum - 1;
    for (int i = 0; i < expectedTask; i++) {
        if (queue->GetLength() == queue->GetCapacity()) {
            return task;
        }

        CPUEUTask* task2local = sched.PickNextTask();
        if (task2local == nullptr) {
            return task;
        }

        queue->PushTail(task2local);
    }

    return task;
}

bool InsertTask(void* task, int qos)
{
    ffrt_executor_task_t* task_ = (ffrt_executor_task_t *)task;
    LinkedList* node = reinterpret_cast<LinkedList *>(&task_->wq);
    return FFRTScheduler::Instance()->InsertNode(node, ffrt::QoS(qos));
}

void CPUWorkerManager::TryMoveLocal2Global(WorkerThread* thread)
{
    if (tearDown) {
        return;
    }

    SpmcQueue* queue = &(reinterpret_cast<CPUWorker*>(thread)->localFifo);
    if (queue->GetLength() == queue->GetCapacity()) {
        queue->PopHeadToGlobalQueue(queue->GetLength() / 2, thread->GetQos(), InsertTask);
    }
}

unsigned int CPUWorkerManager::StealTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return 0;
    }

    if (GetStealingWorkers(thread->GetQos()) > groupCtl[thread->GetQos()].threads.size() / 2) {
        return 0;
    }

    std::shared_lock<std::shared_mutex> lck(groupCtl[thread->GetQos()].tgMutex);
    AddStealingWorker(thread->GetQos());
    std::unordered_map<WorkerThread*, std::unique_ptr<WorkerThread>>::iterator iter =
        groupCtl[thread->GetQos()].threads.begin();
    while (iter != groupCtl[thread->GetQos()].threads.end()) {
        SpmcQueue* queue = &(reinterpret_cast<CPUWorker*>(iter->first)->localFifo);
        unsigned int queueLen = queue->GetLength();
        if (iter->first != thread && queueLen > 0) {
            unsigned int popLen = queue->PopHeadToAnotherQueue(
                reinterpret_cast<CPUWorker*>(thread)->localFifo, (queueLen + 1) / 2);
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
    if (tearDown || PollerProxy::Instance()->GetPoller(thread->GetQos()).DetermineEmptyMap()) {
        return PollerRet::RET_NULL;
    }
    auto& pollerMtx = pollersMtx[thread->GetQos()];
    if (pollerMtx.try_lock()) {
        polling_[thread->GetQos()] = 1;
        if (timeout == -1) {
            monitor->IntoPollWait(thread->GetQos());
        }
        PollerRet ret = PollerProxy::Instance()->GetPoller(thread->GetQos()).PollOnce(timeout);
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
#endif

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
        thread->SetWorkerBlocked(false);
        thread->SetExited(true);
        thread->Detach();
        auto worker = std::move(groupCtl[qos].threads[thread]);
        size_t ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(QoS(qos), pid);
        worker = nullptr;
    }
}

void CPUWorkerManager::NotifyTaskAdded(const QoS& qos)
{
    monitor->Notify(qos, TaskNotifyType::TASK_ADDED);
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
} // namespace ffrt
