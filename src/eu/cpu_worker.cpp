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

#include "cpu_worker.h"
#include "eu/worker_thread.h"
#include "ffrt_trace.h"
#include "sched/scheduler.h"
#include "eu/cpu_manager_interface.h"
#include "dfx/bbox/bbox.h"
#include "eu/func_manager.h"
#include "core/dependence_manager.h"
#ifdef FFRT_IO_TASK_SCHEDULER
#include "sync/poller.h"
#include "queue/queue.h"
#endif

namespace ffrt {
void CPUWorker::Run(TaskCtx* task)
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL2, Run);
    if constexpr(USE_COROUTINE) {
        CoStart(task);
    } else {
        auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
        auto exp = ffrt::SkipStatus::SUBMITTED;
        if (likely(__atomic_compare_exchange_n(&task->skipped, &exp, ffrt::SkipStatus::EXECUTED, 0,
            __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))) {
            FFRT_TASK_BEGIN(task->label, task->gid);
            f->exec(f);
            FFRT_TASK_END();
        }
        f->destroy(f);
        task->UpdateState(ffrt::TaskState::EXITED);
    }
}

void CPUWorker::Run(ffrt_executor_task_t* task, ffrt_qos_t qos)
{
#ifdef FFRT_BBOX_ENABLE
    TaskRunCounterInc();
#endif
    ffrt_executor_task_func func = nullptr;
    ffrt_executor_task_type_t type = static_cast<ffrt_executor_task_type_t>(task->type);
    if (task->type == ffrt_io_task) {
        func = FuncManager::Instance()->getFunc(ffrt_io_task);
    } else {
        func = FuncManager::Instance()->getFunc(ffrt_uv_task);
    }
    if (func == nullptr) {
        FFRT_LOGE("Static func is nullptr");
        return;
    }
    FFRT_EXECUTOR_TASK_BEGIN(task);
    func(task, qos);
    FFRT_EXECUTOR_TASK_END();
    if (type != ffrt_io_task) {
        FFRT_EXECUTOR_TASK_FINISH_MARKER(task); // task finish marker for uv task
    }
#ifdef FFRT_BBOX_ENABLE
    TaskFinishCounterInc();
#endif
}

#ifdef FFRT_IO_TASK_SCHEDULER
void CPUWorker::RunTask(ffrt_executor_task_t* curtask, CPUWorker* worker, TaskCtx* &lastTask)
{
    auto ctx = ExecuteCtx::Cur();
    if (curtask->type != 0) {
        ctx->exec_task = curtask;
        Run(curtask, (int)worker->GetQos());
        ctx->exec_task = nullptr;
    } else {
        TaskCtx* task = reinterpret_cast<TaskCtx*>(curtask);
        UserSpaceLoadRecord::UpdateTaskSwitch(lastTask, task);
        FFRT_LOGD("EU pick task[%lu]", task->gid);
        task->UpdateState(TaskState::RUNNING);

        lastTask = task;
        ctx->task = task;
        worker->curTask = task;
        Run(task);
        worker->curTask = nullptr;
        ctx->task = nullptr;
    }
}

void CPUWorker::RunTaskLifo(ffrt_executor_task_t* task, CPUWorker* worker, TaskCtx* &lastTask)
{
    RunTask(task, worker, lastTask);
    int lifo_count = 0;
    while (worker->priority_task) {
        lifo_count++;
        ffrt_executor_task_t* task = (ffrt_executor_task_t*)(worker->priority_task);
        worker->priority_task = nullptr;
        RunTask(task, worker, lastTask);
        if (lifo_count > worker->budget) break;
    }
}

void* CPUWorker::GetTask(CPUWorker* worker)
{
    if (worker->tick % worker->global_interval == 0) {
        worker->tick = 0;
        void* task = worker->ops.PickUpTaskBatch(worker);
        if (task == nullptr) {
            return nullptr;
        }
        worker->ops.NotifyTaskPicked(worker);
        if (task) return task;
    }
    if (worker->priority_task) {
        void* task = worker->priority_task;
        worker->priority_task = nullptr;
        return task;
    }
    // 以后打开worker->ops.TryMoveLocal2Global(worker);
    return queue_pophead(&(worker->local_fifo));
}

bool CPUWorker::LocalEmpty(CPUWorker* worker)
{
    if (worker->priority_task == nullptr && queue_length(&(worker->local_fifo)) == 0) return true;
    return false;
}

void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    ctx->local_fifo = &(worker->local_fifo);
    ctx->priority_task_ptr = &(worker->priority_task);
    TaskCtx* lastTask = nullptr;
    unsigned int buf_len = 0;

    FFRT_LOGD("qos[%d] thread start succ", (int)worker->GetQos());
    for (;;) {
        FFRT_LOGD("task picking");
        void* local_task = GetTask(worker);
        worker->tick++;
        if (local_task) {
            if (worker->tick % 51 == 0) worker->ops.TryPoll(worker, 0);
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(local_task);
            RunTaskLifo(work, worker, lastTask);
            continue;
        }

        PollerRet ret = worker->ops.TryPoll(worker, 0);
        if (ret == PollerRet::RET_EPOLL) {
            continue;
        } else if (ret == PollerRet::RET_TIMER) {
            worker->tick = 0;
            continue;
        }

        TaskCtx* task = worker->ops.PickUpTaskBatch(worker);
        if (task) {
            worker->ops.NotifyTaskPicked(worker);
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(task);
            RunTask(work, worker, lastTask);
            continue;
        }

        ret = worker->ops.TryPoll(worker, 0);
        if (ret == PollerRet::RET_EPOLL) {
            continue;
        } else if (ret == PollerRet::RET_TIMER) {
            worker->tick = 0;
            continue;
        }

        if (queue_length(&(worker->local_fifo)) == 0) {
            buf_len = worker->ops.StealTaskBatch(worker);
        }
        if (!LocalEmpty(worker)) {
            worker->tick = 1;
            continue;
        }

        ret = worker->ops.TryPoll(worker, -1);
        if (ret == PollerRet::RET_EPOLL) {
            continue;
        } else if (ret == PollerRet::RET_TIMER) {
            worker->tick = 0;
            continue;
        }

        FFRT_WORKER_IDLE_BEGIN_MARKER();
        auto action = worker->ops.WaitForNewAction(worker);
        FFRT_WORKER_IDLE_END_MARKER();
        if (action == WorkerAction::RETRY) {
            continue;
        } else if (action == WorkerAction::RETIRE) {
            break;
        }
    }

    CoWorkerExit();
    FFRT_LOGD("ExecutionThread exited");
    queue_destroy(&worker->local_fifo);
    free(worker->steal_buffer);
    worker->ops.WorkerRetired(worker);
}
#else
void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    TaskCtx* lastTask = nullptr;

    FFRT_LOGD("qos[%d] thread start succ", static_cast<int>(worker->GetQos()));
    for (;;) {
        FFRT_LOGD("task picking");
        TaskCtx* task = worker->ops.PickUpTask(worker);
        if (task) {
            worker->ops.NotifyTaskPicked(worker);
        } else {
            FFRT_WORKER_IDLE_BEGIN_MARKER();
            auto action = worker->ops.WaitForNewAction(worker);
            FFRT_WORKER_IDLE_END_MARKER();
            if (action == WorkerAction::RETRY) {
                continue;
            } else if (action == WorkerAction::RETIRE) {
                break;
            }
        }

        BboxCheckAndFreeze();

        if (task->type != 0) {
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(task);
#ifdef FFRT_UV_LOG_ENABLE
            {
                std::unique_lock lock(DependenceManager::Instance()->taskMapMutex_);
                ffrt_executor_task_t submitTask =
                    DependenceManager::Instance()->taskMap_[reinterpret_cast<uintptr_t>(&work->wq)];
                if (work->reserved[0] == 0 || work->reserved[1] == 0 || work->type == 0 ||
                    work->reserved[0] != submitTask.reserved[0] || work->reserved[1] != submitTask.reserved[1] ||
                    work->type != submitTask.type) {
                    FFRT_LOGD("submit uv executor work[%p], done[%p], loop[%p], "
                        "saved uv executor work[%p], done[%p], loop[%p]", work->reserved[0], work->reserved[1],
                        work->type, submitTask.reserved[0], submitTask.reserved[1], submitTask.type);
                }
            }
#endif
            Run(work, static_cast<ffrt_qos_t>(worker->GetQos()));
        } else {
            FFRT_LOGD("EU pick task[%lu]", task->gid);
            UserSpaceLoadRecord::UpdateTaskSwitch(lastTask, task);
            task->UpdateState(TaskState::RUNNING);

            lastTask = task;
            ctx->task = task;
            worker->curTask = task;
            Run(task);
        }
        BboxCheckAndFreeze();
        worker->curTask = nullptr;
        ctx->task = nullptr;
    }

    CoWorkerExit();
    FFRT_LOGD("ExecutionThread exited");
    worker->ops.WorkerRetired(worker);
}
#endif
} // namespace ffrt
