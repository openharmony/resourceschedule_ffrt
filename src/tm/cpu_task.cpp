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
#include "tm/cpu_task.h"
#include <securec.h>
#include "dfx/trace_record/ffrt_trace_record.h"
#include "dm/dependence_manager.h"

#include "internal_inc/osal.h"
#include "tm/task_factory.h"
#include "util/ffrt_facade.h"
#include "util/slab.h"
#include "eu/func_manager.h"

namespace {
const int TSD_SIZE = 128;
}

namespace ffrt {
void CPUEUTask::SetQos(const QoS& newQos)
{
    if (newQos == qos_inherit) {
        if (!this->IsRoot()) {
            this->qos_ = parent->qos_;
        } else {
            this->qos_ = QoS();
        }
        FFRT_LOGD("Change task %s QoS %d", label.c_str(), this->qos_());
    } else {
        this->qos_ = newQos;
    }
}

void CPUEUTask::FreeMem()
{
    BboxCheckAndFreeze();
    // only tasks which called ffrt_poll_ctl may have cached events
    if (pollerEnable) {
        FFRTFacade::GetPPInstance().GetPoller(qos_).ClearCachedEvents(this);
    }
#ifdef FFRT_TASK_LOCAL_ENABLE
    TaskTsdDeconstruct(this);
#endif
    TaskFactory<CPUEUTask>::Free(this);
}

void CPUEUTask::Execute()
{
    FFRT_LOGD("Execute task[%lu], name[%s]", gid, label.c_str());
    FFRTTraceRecord::TaskExecute(&(this->executeTime));
    UpdateState(TaskState::RUNNING);
    auto f = reinterpret_cast<ffrt_function_header_t*>(func_storage);
    auto exp = ffrt::SkipStatus::SUBMITTED;
    if (likely(__atomic_compare_exchange_n(&skipped, &exp, ffrt::SkipStatus::EXECUTED, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))) {
        f->exec(f);
    }
    f->destroy(f);
    FFRT_TASKDONE_MARKER(gid);
    if (!USE_COROUTINE) {
        this->UpdateState(ffrt::TaskState::EXITED);
    } else {
        this->coRoutine->isTaskDone = true;
    }
}

CPUEUTask::CPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id,
    const QoS &qos)
    : parent(parent), rank(id)
{
    this->qos_ = qos;
    fq_we.task = this;
    if (attr && !attr->name_.empty()) {
        label = attr->name_;
    } else if (IsRoot()) {
        label = "root";
    } else if ((parent != nullptr) && parent->IsRoot()) {
        label = "t" + std::to_string(rank);
    } else {
        label = parent->label + "." + std::to_string(rank);
    }

    taskLocal = false;
    tsd = nullptr;
    if (attr && attr->taskLocal_) {
        tsd = (void **)malloc(TSD_SIZE * sizeof(void *));
        if (unlikely(tsd == nullptr)) {
            FFRT_LOGE("task local malloc tsd failed");
            return;
        }
        memset_s(tsd, TSD_SIZE * sizeof(void *), 0, TSD_SIZE * sizeof(void *));
        taskLocal = attr->taskLocal_;
    }
    if (attr) {
        stack_size = std::max(attr->stackSize_, MIN_STACK_SIZE);
    }
}

void ExecuteUVTask(TaskBase* task, QoS qos)
{
    ffrt_executor_task_func func = FuncManager::Instance()->getFunc(ffrt_uv_task);
    if (func == nullptr) {
        FFRT_LOGE("Static func is nullptr");
        return;
    }
    ffrt_executor_task_t* uv_work = reinterpret_cast<ffrt_executor_task_t *>(task);
    FFRTTraceRecord::TaskExecute<ffrt_uv_task>(qos);
    FFRT_EXECUTOR_TASK_BEGIN(uv_work);
    func(uv_work, qos);
    FFRT_EXECUTOR_TASK_END();
    FFRT_EXECUTOR_TASK_FINISH_MARKER(uv_work); // task finish marker for uv task
    FFRTTraceRecord::TaskDone<ffrt_uv_task>(qos);
}

void ExecuteTask(TaskBase* task, QoS qos)
{
    bool isCoTask = IsCoTask(task);

    // set current task info to context
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    if (isCoTask) {
        ctx->task = reinterpret_cast<CPUEUTask *>(task);
        ctx->lastGid_ = task->gid;
    } else {
        ctx->exec_task = task; // for ffrt_wake_coroutine
    }

    // run Task with coroutine
    if (USE_COROUTINE && isCoTask) {
        while (CoStart(static_cast<CPUEUTask*>(task), GetCoEnv()) != 0) {
            usleep(CO_CREATE_RETRY_INTERVAL);
        }
    } else {
    // run task on thread
#ifdef FFRT_ASYNC_STACKTRACE
        if (isCoTask) {
            FFRTSetStackId(task->stackId);
        }
#endif
        if (task->type < ffrt_invalid_task && task->type != ffrt_uv_task) {
            task->Execute();
        } else {
            ExecuteUVTask(task, qos);
        }
    }

    // reset task info in context
    ctx->task = nullptr;
    ctx->exec_task = nullptr;
}
} /* namespace ffrt */