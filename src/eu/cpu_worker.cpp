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

namespace ffrt {
void CPUWorker::Run(TaskCtx* task)
{
    FFRT_TRACE_SCOPE(2, Run);
#ifdef EU_COROUTINE
    CoStart(task);
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

void CPUWorker::Dispatch(CPUWorker* worker)
{
    auto ctx = ExecuteCtx::Cur();
    TaskCtx* lastTask = nullptr;

    FFRT_LOGI("qos[%d] thread start succ", (int)worker->GetQos());
    for (;;) {
        FFRT_LOGI("task picking");
        TaskCtx* task = worker->ops.PickUpTask(worker);
        if (task) {
            FFRT_LOGI("task[%lu] picked", task->gid);
            worker->ops.NotifyTaskPicked(worker);
            FFRT_LOGI("task[%lu] notified", task->gid);
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

        UserSpaceLoadRecord::UpdateTaskSwitch(lastTask, task);

        FFRT_LOGD("EU pick task[%lu]", task->gid);

        task->UpdateState(TaskState::RUNNING);

        lastTask = task;
        ctx->task = task;
        worker->curTask = task;
        Run(task);
        BboxCheckAndFreeze();
        worker->curTask = nullptr;
        ctx->task = nullptr;
    }

    FFRT_LOGD("ExecutionThread exited");
    worker->ops.WorkerRetired(worker);
}

} // namespace ffrt
