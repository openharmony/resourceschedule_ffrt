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
#include "dfx/trace/ffrt_trace.h"
#include "sched/scheduler.h"
#include "eu/cpu_manager_interface.h"
#include "dfx/bbox/bbox.h"
#include "eu/func_manager.h"

namespace ffrt {
void CPUWorker::Run(TaskCtx* task)
{
    FFRT_TRACE_SCOPE(TRACE_LEVEL2, Run);
    if constexpr(USE_COROUTINE) {
        CoStart(task);
    } else {
        auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
        auto exp = ffrt::SkipStatus::SUBMITTED;
        if (likely(__atomic_compare_exchange_n(&task->skipped, &exp, ffrt::SkipStatus::EXECUTED, 0, __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED))) {
            f->exec(f);
        }
        f->destroy(f);
        task->UpdateState(ffrt::TaskState::EXITED);
    }
}

void CPUWorker::Run(ffrt_executor_task_t* task, ffrt_qos_t qos)
{
    ffrt_executor_task_func func = nullptr;
    if (task->type ==ffrt_rust_task)
    {
        func = FuncManager::Instance()->getFunc(ffrt_rust_task);
    } else {
        func = FuncManager::Instance()->getFunc(ffrt_uv_task;
    }

    if (func == nullptr) {
        FFRT_LOGE("func is nullptr");
        return;
    }
    func(task, qos);
}

void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    TaskCtx* lastTask = nullptr;

    FFRT_LOGD("qos[%d] thread start succ", (int)worker->GetQos());
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

        FFRT_LOGD("EU pick task[%lu]", task->gid);

        if (task->type != 0) {
            ffrt_executor_task_t* work = (ffrt_executor_task_t*)task;
            Run(work);
        } else {
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

void CPUWorker::RunTask(ffrt_executor_task_t* curtask, CPUWorker* worker, TaskCtx* &lastTask)
{
    auto ctx = ExecuteCtx::Cur();
    if (curtask->type != 0) {
        ctx->exec_task = curtask;
        Run(curtask);
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

void CPUworker::RunTaskLifo(ffrt_executor_task_t* task,  CPUWorker* worker, TaskCtx* &lastTask)
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
        worker->ops.NotifyTaskPicked(worker);
        return task ? task : queue_pophead(&(worker->local_fifo));
    } else {
        if (worker->priority_task) {
            void* task = worker->priority_task;
            worker->priority_task = nullptr;
            return task;
        }
        return queue_pophead(&(worker->local_fifo));
    }
}

void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    ctx->local_fifo = &(worker->local_fifo);
    ctx->priority_task_ptr = &(worker->priority_task);
    TaskCtx* lastTask = nullptr;
    unsigned int buf_len = 0;

    FFRT_LOGD("qos[%d] thread start succ", (int)worker->GetQos());
    for(;;) {
        FFRT_LOGD("task picking");
        void* local_task = GetTask(worker);
        worker->tick++;
        if (local_task) {
            if (worker->tick % 2 ==0) worker->ops.TryPoll(worker, 0);
            ffrt_executor_task_t* work = (ffrt_executor_task_t*)local_task;
            RunTaskLifo(work, worker, lastTask);
            continue;
        }

        if (worker->ops.TryPoll(worker, 0)) continue;

        TaskCtx* task = worker->ops.PickUpTaskBatch(worker);
        if (task) {
            worker->ops.NotifyTaskPicked(worker);
            ffrt_executor_task_t* work = (ffrt_executor_task_t*)task;
            RunTask(work, worker, lastTask);
            continue;
        }

        if (worker->ops.TryPoll(worker, 0)) continue;

        if (queue_length(&(worker->local_fifo)) == 0) {
            buf_len = worker->ops.StealTaskBatch(worker);
        }
        if (queue_length(&(worker->local_fifo)) != 0) {
            worker->tick = 1;
            continue;
        }

        if (worker->ops.TryPoll(worker, -1)) continue;

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
} // namespace ffrt
