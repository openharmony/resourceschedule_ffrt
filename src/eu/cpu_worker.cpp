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
#include "dm/dependence_manager.h"
#ifdef FFRT_IO_TASK_SCHEDULER
#include "sync/poller.h"
#include "util/spmc_queue.h"
#endif
#include "tm/cpu_task.h"

namespace ffrt {
const int PLACE_HOLDER = 0;
const unsigned int TRY_POLL_FREQ = 51;
}

namespace ffrt {
void CPUWorker::Run(CPUEUTask* task)
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
    if (type == ffrt_io_task) {
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

void* CPUWorker::WarpDispatch(void* worker)
{
    reinterpret_cast<CPUWorker*>(worker)->NativeConfig();
    Dispatch(reinterpret_cast<CPUWorker*>(worker));
    return nullptr;
}

#ifdef FFRT_IO_TASK_SCHEDULER
void CPUWorker::RunTask(ffrt_executor_task_t* curtask, CPUWorker* worker, CPUEUTask* &lastTask)
{
    auto ctx = ExecuteCtx::Cur();
    if (curtask->type != 0) {
        ctx->exec_task = curtask;
        worker->curTask = curtask;
        Run(curtask, static_cast<ffrt_qos_t>(worker->GetQos()));
        worker->curTask = nullptr;
        ctx->exec_task = nullptr;
    } else {
        CPUEUTask* task = reinterpret_cast<CPUEUTask*>(curtask);
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

void CPUWorker::RunTaskLifo(ffrt_executor_task_t* task, CPUWorker* worker, CPUEUTask* &lastTask)
{
    RunTask(task, worker, lastTask);

    unsigned int lifoCount = 0;
    while (worker->priority_task != nullptr && worker->priority_task != &PLACE_HOLDER) {
        lifoCount++;
        ffrt_executor_task_t* priorityTask = reinterpret_cast<ffrt_executor_task_t*>(worker->priority_task);
        // set a placeholder to prevent the task from being placed in the priority again
        worker->priority_task = (lifoCount > worker->budget) ? const_cast<int*>(&PLACE_HOLDER) : nullptr;

        RunTask(priorityTask, worker, lastTask);
    }
}

void* CPUWorker::GetTask(CPUWorker* worker)
{
    // periodically pick up tasks from the global queue to prevent global queue starvation
    if (worker->tick % worker->global_interval == 0) {
        worker->tick = 0;
        void* task = worker->ops.PickUpTaskBatch(worker);
        if (task != nullptr) {
            worker->ops.NotifyTaskPicked(worker);
        }
        return task;
    }

    // preferentially pick up tasks from the priority unless the priority is empty or occupied
    if (worker->priority_task != nullptr) {
        void* task = worker->priority_task;
        worker->priority_task = nullptr;
        if (task != &PLACE_HOLDER) {
            return task;
        }
    }

    return worker->localFifo.PopHead();
}

PollerRet CPUWorker::TryPoll(CPUWorker* worker, int timeout)
{
    PollerRet ret = worker->ops.TryPoll(worker, timeout);
    if (ret == PollerRet::RET_TIMER) {
        worker->tick = 0;
    }

    return ret;
}

bool CPUWorker::LocalEmpty(CPUWorker* worker)
{
    return (worker->priority_task == nullptr) && (worker->localFifo.GetLength() == 0);
}

void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    ctx->localFifo = &(worker->localFifo);
    ctx->priority_task_ptr = &(worker->priority_task);
    ctx->qos = worker->GetQos();
    CPUEUTask* lastTask = nullptr;

    FFRT_LOGD("qos[%d] thread start succ", (int)worker->GetQos());
    for (;;) {
        FFRT_LOGD("task picking");
        // get task in the order of priority -> local queue -> global queue
        void* local_task = GetTask(worker);
        worker->tick++;
        if (local_task) {
            if (worker->tick % TRY_POLL_FREQ == 0) {
                worker->ops.TryPoll(worker, 0);
            }
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(local_task);
            RunTaskLifo(work, worker, lastTask);
            continue;
        }

        PollerRet ret = TryPoll(worker, 0);
        if (ret != PollerRet::RET_NULL) {
            continue;
        }

        // pick up tasks from global queue
        CPUEUTask* task = worker->ops.PickUpTaskBatch(worker);
        if (task) {
            worker->ops.NotifyTaskPicked(worker);
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(task);
            RunTask(work, worker, lastTask);
            continue;
        }

        // check the epoll status again to prevent fd or timer events from being missed
        ret = TryPoll(worker, 0);
        if (ret != PollerRet::RET_NULL) {
            continue;
        }

        if (worker->localFifo.GetLength() == 0) {
            worker->ops.StealTaskBatch(worker);
        }
        if (!LocalEmpty(worker)) {
            worker->tick = 1;
            continue;
        }

        // enable a worker to enter the epoll wait -1 state and continuously listen to fd or timer events
        // only one worker enters this state at a QoS level
        ret = TryPoll(worker, -1);
        if (ret != PollerRet::RET_NULL) {
            continue;
        }

        FFRT_WORKER_IDLE_BEGIN_MARKER();
        auto action = worker->ops.WaitForNewAction(worker);
        FFRT_WORKER_IDLE_END_MARKER();
        if (action == WorkerAction::RETRY) {
            worker->tick = 0;
            continue;
        } else if (action == WorkerAction::RETIRE) {
            break;
        }
    }

    CoWorkerExit();
    FFRT_LOGD("ExecutionThread exited");
    free(worker->steal_buffer);
    worker->ops.WorkerRetired(worker);
}
#else
void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    CPUEUTask* lastTask = nullptr;

    worker->ops.WorkerPrepare(worker);
    FFRT_LOGD("qos[%d] thread start succ", static_cast<int>(worker->GetQos()));
    for (;;) {
        FFRT_LOGD("task picking");
        CPUEUTask* task = worker->ops.PickUpTask(worker);
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
            worker->curTask = task;
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(task);
            Run(work, static_cast<ffrt_qos_t>(worker->GetQos()));
        } else {
            FFRT_LOGD("EU pick task[%lu]", task->gid);
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
