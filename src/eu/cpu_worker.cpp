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
#include "eu/cpu_manager_strategy.h"
#include "dfx/bbox/bbox.h"
#include "eu/func_manager.h"
#include "dm/dependence_manager.h"
#include "dfx/perf/ffrt_perf.h"
#include "sync/poller.h"
#include "util/ffrt_facade.h"
#include "tm/cpu_task.h"
#include "tm/queue_task.h"
#include "eu/cpuworker_manager.h"
#include "dfx/sysevent/sysevent.h"
namespace {
int PLACE_HOLDER = 0;
const unsigned int TRY_POLL_FREQ = 51;
}

namespace ffrt {
void* CPUWorker::WrapDispatch(void* worker)
{
    reinterpret_cast<CPUWorker*>(worker)->NativeConfig();
    Dispatch(reinterpret_cast<CPUWorker*>(worker));
    return nullptr;
}

void CPUWorker::RunTask(TaskBase* task, CPUWorker* worker)
{
    bool isNotUv = task->type == ffrt_normal_task || task->type == ffrt_queue_task;
#ifdef FFRT_SEND_EVENT
    static bool isBetaVersion = IsBeta();
    uint64_t startExecuteTime = 0;
    if (isBetaVersion) {
        startExecuteTime = FFRTTraceRecord::TimeStamp();
        CPUEUTask* cpu_task = reinterpret_cast<CPUEUTask*>(task);
        if (likely(isNotUv)) {
            worker->cacheLabel = cpu_task->label;
        }
    }
#endif
    worker->curTask = task;
    worker->curTaskType_ = task->type;
#ifdef WORKER_CACHE_TASKNAMEID
    if (isNotUv) {
        worker->curTaskLabel_ = task->GetLabel();
        worker->curTaskGid_ = task->gid;
    }
#endif

    ExecuteTask(task, worker->GetQos());

    worker->curTask = nullptr;
    worker->curTaskType_ = ffrt_invalid_task;
#ifdef FFRT_SEND_EVENT
    if (isBetaVersion) {
        uint64_t execDur = ((FFRTTraceRecord::TimeStamp() - startExecuteTime) / worker->cacheFreq);
        TaskBlockInfoReport(execDur, isNotUv ? worker->cacheLabel : "uv_task", worker->cacheQos, worker->cacheFreq);
    }
#endif
}

void CPUWorker::RunTaskLifo(TaskBase* task, CPUWorker* worker)
{
    RunTask(task, worker);

    unsigned int lifoCount = 0;
    while (worker->priority_task != nullptr && worker->priority_task != &PLACE_HOLDER) {
        lifoCount++;
        TaskBase* priorityTask = reinterpret_cast<TaskBase*>(worker->priority_task);
        // set a placeholder to prevent the task from being placed in the priority again
        worker->priority_task = (lifoCount > worker->budget) ? &PLACE_HOLDER : nullptr;

        RunTask(priorityTask, worker);
    }
}

void* CPUWorker::GetTask(CPUWorker* worker)
{
#ifdef FFRT_LOCAL_QUEUE_ENABLE
    // periodically pick up tasks from the global queue to prevent global queue starvation
    if (worker->tick % worker->global_interval == 0) {
        worker->tick = 0;
        TaskBase* task = static_cast<TaskBase*>(worker->ops.PickUpTaskBatch(worker));
        // the worker is not notified when the task attribute is set not to notify worker
        if (NeedNotifyWorker(task)) {
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
#else
    TaskBase* task = worker->ops.PickUpTaskBatch(worker);
    if (task != nullptr) {
        worker->ops.NotifyTaskPicked(worker);
    }

    return task;
#endif
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
    worker->WorkerSetup();

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    if (worker->ops.IsBlockAwareInit()) {
        int ret = BlockawareRegister(worker->GetDomainId());
        if (ret != 0) {
            FFRT_SYSEVENT_LOGE("blockaware register fail, ret[%d]", ret);
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
    WorkerLooperDefault(worker);
    CoWorkerExit();
    worker->ops.WorkerRetired(worker);
}

// work looper which inherited from history
void CPUWorker::WorkerLooperDefault(CPUWorker* worker)
{
    const sched_mode_type& schedMode = CPUManagerStrategy::GetSchedMode(worker->GetQos());
    for (;;) {
        // get task in the order of priority -> local queue -> global queue
        TaskBase* local_task = reinterpret_cast<TaskBase*>(GetTask(worker));
        worker->tick++;
        if (local_task) {
            if (worker->tick % TRY_POLL_FREQ == 0) {
                worker->ops.TryPoll(worker, 0);
            }
            goto run_task;
        }

        if (schedMode == sched_mode_type::sched_default_mode) {
            goto poll_once;
        } else {
            // direct to pollwait when no task available
            goto poll_wait;
        }

run_task:
        RunTaskLifo(local_task, worker);
        continue;

poll_once:
        if (TryPoll(worker, 0) != PollerRet::RET_NULL) {
            continue;
        }

#ifdef FFRT_LOCAL_QUEUE_ENABLE
        // pick up tasks from global queue
        local_task = static_cast<TaskBase*>(worker->ops.PickUpTaskBatch(worker));
        // the worker is not notified when the task attribute is set not to notify worker
        if (local_task != nullptr) {
            if (NeedNotifyWorker(local_task)) {
                worker->ops.NotifyTaskPicked(worker);
            }
            RunTask(local_task, worker);
            continue;
        }

        // check the epoll status again to prevent fd or timer events from being missed
        if (TryPoll(worker, 0) != PollerRet::RET_NULL) {
            continue;
        }

        if (worker->localFifo.GetLength() == 0) {
            worker->ops.StealTaskBatch(worker);
        }

        if (!LocalEmpty(worker)) {
            worker->tick = 1;
            continue;
        }
#endif

poll_wait:
        // enable a worker to enter the epoll wait -1 state and continuously listen to fd or timer events
        // only one worker enters this state at a QoS level
        if (TryPoll(worker, -1) != PollerRet::RET_NULL) {
            continue;
        }

        FFRT_PERF_WORKER_IDLE(static_cast<int>(worker->qos));
        auto action = worker->ops.WaitForNewAction(worker);
        if (action == WorkerAction::RETRY) {
            FFRT_PERF_WORKER_AWAKE(static_cast<int>(worker->qos));
            worker->tick = 0;
            continue;
        } else if (action == WorkerAction::RETIRE) {
            break;
        }
    }
}
} // namespace ffrt