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
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "eu/qos_interface.h"
#include "eu/cpuworker_manager.h"
#include "queue/queue.h"

namespace ffrt {

bool CPUWorkerManager::IncWorker(const QoS& qos)
{
    std::unique_lock lock(groupCtl[qos()].tgMutex);
    if (tearDown) {
        return false;
    }

    auto worker = std::unique_ptr<WorkerThread>(new (std::nothrow) CPUWorker(qos, {
        std::bind(&CPUWorkerManager::PickUpTask, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::NotifyTaskPicked, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerIdleAction, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerRetired, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::TryPoll, this, std::placeholders::_1, std::placeholders::_2),
        std::bind(&CPUWorkerManager::StealTask, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::StealTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::PickUpTaskBatch, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::TryMoveLocal2Global, this, std::placeholders::_1),
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
    TaskCtx* task = sched.PickNextTask();
    return task;
}

TaskCtx* CPUWorkerManager::PickUpTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }
    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    if (((CPUWorker *)thread)->priority_task == nullptr) {
        ((CPUWorker *)thread)->priority_task = sched.PickNextTask();
    }
    if (((CPUWorker *)thread)->priority_task == nullptr) {
        return nullptr;
    }

    SubStealingWorker(thread->GetQos());
    unsigned int expected_task = (GetTaskCount(thread->GetQos()) / monitor.WakedWorkerNum(thread->GetQos()));
    for (int i = 0; i < expected_task; i++) {
        TaskCtx* task = sched.PickNextTask();
        if (task == nullptr) {
            return nullptr;
        }
        if (queue_pushtail(&(((CPUWorker *)thread)->local_fifo), task) == ERROR_QUEUE_FULL) {
            return task;
        }
    }
    TaskCtx* task = reinterpret_cast<TaskCtx*>(((CPUWorker *)thread)->priority_task);
    ((CPUWorker *)thread)->priority_task = nullptr;
    return task;
}

void CPUWorkerManager::TryMoveLocal2Global(WorkerThread* thread)
{
    if (tearDown) {
        return;
    }
    struct queue_s *queue = &(((CPUWorker *)(thread))->local_fifo);
    if (queue_length(queue) == queue_capacity(queue)) {
        unsigned int buf_len = queue_pophead_batch(queue,
            (((CPUWorker *)thread)->steal_buffer), queue_length(queue) / 2);
        for (int i = buf_len - 1; i >=0; --i) {
            ffrt_executor_task* task = (ffrt_executor_task*)(((CPUWorker *)thread)->steal_buffer[i]);
            if (!FFRTScheduler::Instance()->InsertNode((ffrt::LinkedList *)(&task->wq), thread->GetQos())) {
                FFRT_LOGE("Submit RUST task failed!");
            }
        }
    }
    
}

void* CPUWorkerManager::StealTask(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }
    std::unordered_map<WorkerThread*, std::unique_ptr<WorkerThread>>::iterator iter =
        groupCtl[thread->GetQos()].threads.begin();
    while (iter != groupCtl[thread->GetQos()].threads.end()) {
        if (iter->first != thread && queue_prob(&((CPUWorker *)(iter->first))->local_fifo) > 0) {
            return queue_pophead(&((CPUWorker *)(iter->first))->local_fifo);
        }
        iter++;
    }
    return nullptr;
}

unsigned int CPUWorkerManager::StealTaskBatch(WorkerThread* thread)
{
    if (tearDown) {
        return 0;
    }
    static_assert(STEAL_BUFFER_SIZE == LOCAL_QUEUE_SIZE / 2);
    if (GetStealingWorkers(thread->GetQos()) > groupCtl[thread->GetQos()].threads.size() / 2) {
        return 0;
    }
    AddStealingWorker(thread->GetQos());
    std::unordered_map<WorkerThread*, std::unique_ptr<WorkerThread>>::iterator iter =
        groupCtl[thread->GetQos()].threads.begin();
    while (iter != groupCtl[thread->GetQos()].threads.end()) {
        struct queue_s *queue = &(((CPUWorker *)(thread))->local_fifo);
        if (iter->first != thread && queue_length(queue) > 1) {
            unsigned int buf_len = queue_pophead_batch(queue, (((CPUWorker *)thread)->steal_buffer), queue_length(queue) / 2);
            queue_pushtail_batch(&(((CPUWorker *)thread)->local_fifo), ((CPUWorker *)thread)->steal_buffer, buf_len);
            return buf_len;
        }
        iter++;
    }
    return 0;
}

void CPUWorkerManager::NotifyTaskPicked(const WorkerThread* thread)
{
    monitor.Notify(thread->GetQos(), TaskNotifyType::TASK_PICKED);
}

void CPUWorkerManager::WorkerRetired(WorkerThread* thread)
{
    pid_t pid = thread->Id();
    int qos = static_cast<int>(thread->GetQos());
    thread->SetExited(true);
    thread->Detach();

    {
        std::unique_lock lock(groupCtl[qos].tgMutex);
        auto worker = std::move(groupCtl[qos].threads[thread]);
        size_t ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(qos, pid);
        worker = nullptr;
    }
}

bool CPUWorkerManager::TryPoll(const WorkerThread* thread, int timeout)
{
    auto& pollerMtx = pollersMtx[thread->GetQos()];
    if (!pollersExitFlag[thread->GetQos()].load(std::memory_order_relaxed) && pollerMtx.try_lock()) {
        bool ret = PollerProxy::Instance()->GetPoller(thread->GetQos()).PollOnce(timeout);
        pollerMtx.unlock();
        return ret;
    }
    return false;
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
    if (ctl.cv.wait_for(lk, std::chrono::seconds(5),
        [this, thread] {return tearDown || GetTaskCount(thread->GetQos())
            || ((CPUWorker *)thread)->priority_task || queue_length(&(((CPUWorker *)thread)->local_fifo));})) {
        monitor.WakeupCount(thread->GetQos());
        FFRT_LOGD("worker awake");
        return WorkerAction::RETRY;
    } else {
        monitor.TimeoutCount(thread->GetQos());
        FFRT_LOGD("worker exit");
        return WorkerAction::RETIRE;
    }
#else /* !IDLE_WORKER_DESTRUCT */
    ctl.cv.wait(lk, [this, thread] {return tearDown || GetTaskCount(thread->GetQos())
        || ((CPUWorker *)thread)->priority_task || queue_length(&(((CPUWorker *)thread)->local_fifo));});
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
    if (stealWorkers[qos()].load(std::memory_order_relaxed) == 0){
        monitor.Notify(qos, TaskNotifyType::TASK_LOCAL);
    }
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
