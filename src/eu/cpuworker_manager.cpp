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
#ifdef FFER_IO_TASK_SCHEDULER
#include "queue/queue.h"
#endif
#include "eu/cpu_manager_interface.h"
#include "eu/cpu_monitor.h"
#include "eu/qos_interface.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "eu/cpuworker_manager.h"

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
#ifdef FFRT_IO_TASK_SCHEDULER
        std::bind(&CPUWorkerManager::TryPoll, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&CPUWorkerManager::StealTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::PickUpTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::TryMoveLocal2Global, this, std::placeholders::_1),
#endif
    }));
    if (worker == nullptr) {
        FFRT_LOGE("Inc CPUWorker: create worker\n");
        return false;
    }
    worker->WorkerSetup(worker.get(), qos);
    WorkerJoinTg(qos, worker->Id());
    groupCtl[qos()].threads[worker.get()] = std::move(worker);
    return true;
}

void CPUWorkerManager::WakeupWorkers(const QoS& qos)
{
    if (tearDown) {
        return;
    }

    auto& ctl = sleepCtl[qos()];
    ctl.cv.notify_one();
}

int CPUWorkerManager::GetTaskCount(const QoS& qos)
{
    auto& sched = FFRTScheduler::Instance()->GetScheduler(qos);
    return sched.RQSize();
}

TaskCtx* CPUWorkerManager::PickUpTask(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }

    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    return sched.PickNextTask();
}

#ifdef FFRT_IO_TASK_SCHEDULER
TaskCtx* CPUWorkerManager::PickUpTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }
    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    TaskCtx* task = sched.PickNextTask();
    if (task == nullptr) return nullptr;
    struct queue_s *queue = &(((CPUWorker *)thread)->local_fifo);
    int expected_task = (GetTaskCount(thread->GetQos()) / monitor.WakedWorkerNum(thread->GetQos()));
    for (int i = 0; i <expected_task; i++) {
        if (queue_length(queue) == queue_capacity(queue)) {
            return task;
        }
        TaskCtx* task2local = sched.PickNextTask();
        if (task2local == nullptr) {
            return task;
        }
        if (((CPUWorker *)thread)->priority_task == nullptr) {
            ((CPUWorker *)thread)->priority_task = task2local;
        } else {
            int ret = queue_pushtail(queue, task2local);
        }
    }
    return task;
}

bool InsertTask(void *task, int qos)
{
    ffrt_executor_task_t* task = (ffrt_executor_task_t)task;
    LinkedList* node = (LinkedList *)(&task->wq);
    return FFRTScheduler::Instance()->InsertNode(node, ffrt::Qos(qos);)
}

void CPUWorkerManager::TryMoveLocal2Global(WorkerThread* thread)
{
    if (tearDown) {
        return;
    }
    struct queue_s *queue = &(((CPUWorker *)(thread))->local_fifo);
    if (queue_length(queue) == queue_capacity(queue)) {
        queue_pophead_to_gqueue_batch(queue, queue_length(queue) / 2, thread->GetQos(), InsertTask);
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
        struct queue_s *queue = &(((CPUWorker *)(iter->first))->local_fifo);
        unsigned int queue_len = queue_length(queue);
        if (iter->first != thread && queue_len > 1) {
            unsigned int buf_len = queue_pophead_pushtail_batch(queue, &(((CPUWorker *)(thread))->local_fifo),
                queue_len /2);
            SubStealingWorker(thread->GetQos());
            return buf_len;
        }
        iter++;
    }
    SubStealingWorker(thread->GetQos());
    return 0;
}

PollerRet CPUWorkerManager::TryPoll(const WorkerThread* thread, int timeout)
{
    if (tearDown) {
        return PollerRet::RET_NULL;
    }

    auto& pollerMtx = pollersMtx[thread->GetQos()];
    if (pollerMtx.try_lock()) {
        if (timeout == -1) {
            monitor.IntoPollWait(thread->GetQos());
        }
        PollerRet ret = PollerProxy::Instance()->GetPoller(thread->GetQos()).PollOnce(timeout);
        if (timeout == -1) {
            monitor.OutOfPollWait(thread->GetQos());
        }
        pollerMtx.unlock();
        return ret;
    }
    return PollerRet::RET_NULL;
}

void CPUWorkerManager::NotifyLocalTaskAdded(const QoS& qos)
{
    if (stealWorkers[qos()].load(std::memory_order_relaxed) == 0) {
        monitor.Notify(qos, TaskNotifyType::TASK_LOCAL);
    }
}
#endif

void CPUWorkerManager::NotifyTaskPicked(const WorkerThread* thread)
{
    monitor.Notify(thread->GetQos(), TaskNotifyType::TASK_PICKED);
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
        size_t ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(qos, pid);
        worker = nullptr;
    }
}

WorkerAction CPUWorkerManager::WorkerIdleAction(const WorkerThread* thread)
{
    if (tearDown) {
        return WorkerAction::RETIRE;
    }

    auto& ctl = sleepCtl[thread->GetQos()];
    std::unique_lock lk(ctl.mutex);
    monitor.IntoSleep(thread->GetQos());
    FFRT_LOGD("worker sleep");
#if defined(IDLE_WORKER_DESTRUCT)
#ifdef FFRT_IO_TASK_SCHEDULER
    if (ctl.cv.wait_for(lk, std::chrono::seconds(5), [this, thread] {
        return tearDown || GetTaskCount(thread->GetQos()) || ((CPUWorker *)thread)->priority_task ||
        queue_length(&(((CPUWorker *)thread)->local_fifo));
        })) {
#else
    if (ctl.cv.wait_for(lk, std::chrono::seconds(5), [this, thread] {
        return tearDown || GetTaskCount(thread->GetQos());})) {
#endif
        monitor.WakeupCount(thread->GetQos());
        FFRT_LOGD("worker awake");
        return WorkerAction::RETRY;
    } else {
        monitor.TimeoutCount(thread->GetQos());
        FFRT_LOGD("worker exit");
        return WorkerAction::RETIRE;
    }
#else
#ifdef FFRT_IO_TASK_SCHEDULER
    ctl.cv.wait(lk, [this, thread] {
        return tearDown || GetTaskCount(thread->GetQos()) ||
        ((CPUWorker *)thread)->priority_task || queue_length(&(((CPUWorker *)thread)->local_fifo));
        });
#else
    ctl.cv.wait(lk, [this, thread] {
        return tearDown || GetTaskCount(thread->GetQos());});
#endif
    monitor.WakeupCount(thread->GetQos());
    FFRT_LOGD("worker awake");
    return WorkerAction::RETRY;
#endif /* IDLE_WORKER_DESTRUCT */
}

void CPUWorkerManager::NotifyTaskAdded(const QoS& qos)
{
    monitor.Notify(qos, TaskNotifyType::TASK_ADDED);
}

void CPUWorkerManager::NotifyLocalTaskAdded(const QoS& qos)
{
    if (stealWorkers[qos()].load(std::memory_order_relaxed) == 0) {
        monitor.Notify(qos, TaskNotifyType::TASK_LOCAL);
    }
}

void CPUWorkerManager::NotifyTaskAdded(const QoS& qos)
{
    monitor.Notify(qos, TaskNotifyType::TASK_ADDED);
}

CPUWorkerManager::CPUWorkerManager() : monitor({
    std::bind(&CPUWorkerManager::IncWorker, this, std::placeholders::_1),
    std::bind(&CPUWorkerManager::WakeupWorkers, this, std::placeholders::_1),
    std::bind(&CPUWorkerManager::GetTaskCount, this, std::placeholders::_1)})
{
    groupCtl[qos_deadline_request].tg = std::unique_ptr<ThreadGroup>(new ThreadGroup());
}

void CPUWorkerManager::WorkerJoinTg(const QoS& qos, pid_t pid)
{
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
