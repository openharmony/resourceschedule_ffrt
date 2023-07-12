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
#ifdef EU_COROUTINE
    if (task->coroutine_type == ffrt_coroutine_stackfull) {
        CoStart(task);
    }
    else {
        StacklessCouroutineStart(task);
    }

#else
    auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
    auto exp = ffrt::SkipStatus::SUBMITTED;
    if (likely(__atomic_compare_exchange_n(&task->skipped, &exp, ffrt::SkipStatus::EXECUTED, 0, __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED))) {
        f->exec(f);
    }
    f->destroy(f);

    task->UpdateState(ffrt::TaskState::EXITED);
#endif
}

void CPUWorker::Run(ffrt_executor_task_t* data)
{
#ifdef FFRT_BBOX_ENABLE
    TaskRunCounterInc();
#endif
    ffrt_executor_task_func func = FuncManager::Instance()->getFunc("uv");
    if (func == nullptr) {
        FFRT_LOGE("func is nullptr");
        return;
    }
    func(data);
#ifdef FFRT_BBOX_ENABLE
    TaskFinishCounterInc();
#endif
}

void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    TaskCtx* lastTask = nullptr;

    FFRT_LOGI("qos[%d] thread start succ", (int)worker->GetQos());
    for (;;) {
        FFRT_LOGI("task picking");
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
} // namespace ffrt
