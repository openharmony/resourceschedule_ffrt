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
#include "dfx/perf/ffrt_perf.h"
#include "sync/poller.h"
#include "util/spmc_queue.h"
#include "tm/cpu_task.h"
#include "tm/queue_task.h"
#ifdef FFRT_ASYNC_STACKTRACE
#include "dfx/async_stack/ffrt_async_stack.h"
#endif
namespace {
int PLACE_HOLDER = 0;
const unsigned int TRY_POLL_FREQ = 51;
}

namespace ffrt {
void CPUWorker::Run(CPUEUTask* task)
{
    if constexpr(USE_COROUTINE) {
        CoStart(task);
        return;
    }

    switch (task->type) {
        case ffrt_normal_task: {
#ifdef FFRT_ASYNC_STACKTRACE
            FFRTSetStackId(task->stackId);
#endif
            task->Execute();
            break;
        }
        case ffrt_queue_task: {
            QueueTask* sTask = reinterpret_cast<QueueTask*>(task);
#ifdef FFRT_ASYNC_STACKTRACE
            FFRTSetStackId(sTask->stackId);
#endif
            sTask->IncDeleteRef();
            sTask->Execute();
            sTask->DecDeleteRef();
            break;
        }
        default: {
            FFRT_LOGE("run unsupport task[%lu], type=%d, name[%s]", task->gid, task->type, task->label.c_str());
            break;
        }
    }
}

void CPUWorker::Run(ffrt_executor_task_t* task, ffrt_qos_t qos)
{
#ifdef FFRT_BBOX_ENABLE
    TaskRunCounterInc();
#endif
    if (task == nullptr) {
        FFRT_LOGE("task is nullptr");
        return;
    }
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

void* CPUWorker::WrapDispatch(void* worker)
{
    reinterpret_cast<CPUWorker*>(worker)->NativeConfig();
    Dispatch(reinterpret_cast<CPUWorker*>(worker));
    return nullptr;
}

void CPUWorker::RunTask(ffrt_executor_task_t* curtask, CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    CPUEUTask* task = reinterpret_cast<CPUEUTask*>(curtask);
    worker->curTask = task;
    switch (curtask->type) {
        case ffrt_normal_task:
        case ffrt_queue_task: {
            ctx->task = task;
            Run(task);
            ctx->task = nullptr;
            break;
        }
        default: {
            ctx->exec_task = curtask;
            Run(curtask, static_cast<ffrt_qos_t>(worker->GetQos()));
            ctx->exec_task = nullptr;
            break;
        }
    }
    worker->curTask = nullptr;
}

void CPUWorker::RunTaskLifo(ffrt_executor_task_t* task, CPUWorker* worker)
{
    RunTask(task, worker);

    unsigned int lifoCount = 0;
    while (worker->priority_task != nullptr && worker->priority_task != &PLACE_HOLDER) {
        lifoCount++;
        ffrt_executor_task_t* priorityTask = reinterpret_cast<ffrt_executor_task_t*>(worker->priority_task);
        // set a placeholder to prevent the task from being placed in the priority again
        worker->priority_task = (lifoCount > worker->budget) ? &PLACE_HOLDER : nullptr;

        RunTask(priorityTask, worker);
    }
}

void* CPUWorker::GetTask(CPUWorker* worker)
{
    // periodically pick up tasks from the global queue to prevent global queue starvation
    if (worker->tick % worker->global_interval == 0) {
        worker->tick = 0;
        CPUEUTask* task = worker->ops.PickUpTaskBatch(worker);
        // the worker is not notified when the task attribute is set not to notify worker
        if (task != nullptr) {
            if (task->type == ffrt_normal_task && !task->notifyWorker_) {
                task->notifyWorker_ = true;
                return task;
            }
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
    return ((worker->priority_task == nullptr) && (worker->localFifo.GetLength() == 0));
}

void CPUWorker::Dispatch(CPUWorker* worker)
{
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    if (worker->ops.IsBlockAwareInit()) {
        int ret = BlockawareRegister(worker->GetDomainId());
        if (ret != 0) {
            FFRT_LOGE("blockaware register fail, ret[%d]", ret);
        }
    }
#endif
    auto ctx = ExecuteCtx::Cur();
    ctx->localFifo = &(worker->localFifo);
    ctx->priority_task_ptr = &(worker->priority_task);
    ctx->qos = worker->GetQos();

    worker->ops.WorkerPrepare(worker);
#ifndef OHOS_STANDARD_SYSTEM
    FFRT_LOGI("qos[%d] thread start succ", static_cast<int>(worker->GetQos()));
#endif
    FFRT_PERF_WORKER_AWAKE(static_cast<int>(worker->GetQos()));
    worker->ops.WorkerLooper(worker);
    CoWorkerExit();
    FFRT_LOGD("ExecutionThread exited");
    worker->ops.WorkerRetired(worker);
}

// work looper which inherited from history
void CPUWorker::WorkerLooperDefault(WorkerThread* p)
{
    CPUWorker* worker = reinterpret_cast<CPUWorker*>(p);
    for (;;) {
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        if (!worker->ops.IsExceedRunningThreshold(worker)) {
#endif
        // get task in the order of priority -> local queue -> global queue
        void* local_task = GetTask(worker);
        worker->tick++;
        if (local_task) {
            if (worker->tick % TRY_POLL_FREQ == 0) {
                worker->ops.TryPoll(worker, 0);
            }
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(local_task);
            RunTaskLifo(work, worker);
            continue;
        }

        PollerRet ret = TryPoll(worker, 0);
        if (ret != PollerRet::RET_NULL) {
            continue;
        }

        // pick up tasks from global queue
        CPUEUTask* task = worker->ops.PickUpTaskBatch(worker);
        // the worker is not notified when the task attribute is set not to notify worker
        if (task != nullptr) {
            if (task->type == ffrt_normal_task && !task->notifyWorker_) {
                task->notifyWorker_ = true;
            } else {
                worker->ops.NotifyTaskPicked(worker);
            }
            ffrt_executor_task_t* work = reinterpret_cast<ffrt_executor_task_t*>(task);
            RunTask(work, worker);
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
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        }
#endif
        auto action = worker->ops.WaitForNewAction(worker);
        if (action == WorkerAction::RETRY) {
            worker->tick = 0;
            continue;
        } else if (action == WorkerAction::RETIRE) {
            break;
        }
    }
}

// work looper with standard procedure which could be strategical
void CPUWorker::WorkerLooperStandard(WorkerThread* p)
{
    CPUWorker* worker = reinterpret_cast<CPUWorker*>(p);
    for (;;) {
        // try get task
        CPUEUTask* task = worker->ops.PickUpTask(worker);

        // if succ, notify picked and run task
        if (task != nullptr) {
            worker->ops.NotifyTaskPicked(worker);
            RunTask(reinterpret_cast<ffrt_executor_task_t*>(task), worker);
            continue;
        }
        // otherwise, worker wait action
        auto action = worker->ops.WaitForNewAction(worker);
        if (action == WorkerAction::RETRY) {
            continue;
        } else if (action == WorkerAction::RETIRE) {
            break;
        }
    }
}
} // namespace ffrt
