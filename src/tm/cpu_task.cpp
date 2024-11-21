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

namespace {
const int TSD_SIZE = 128;
}

namespace ffrt {
void CPUEUTask::SetQos(const QoS& newQos)
{
    if (newQos == qos_inherit) {
        if (!this->IsRoot()) {
            this->qos = parent->qos;
        } else {
            this->qos = QoS();
        }
        FFRT_LOGD("Change task %s QoS %d", label.c_str(), this->qos());
    } else {
        this->qos = newQos;
    }
}

void CPUEUTask::FreeMem()
{
    BboxCheckAndFreeze();
    FFRTFacade::GetPPInstance().GetPoller(qos).ClearCachedEvents(this);
#ifdef FFRT_TASK_LOCAL_ENABLE
    TaskTsdDeconstruct(this);
#endif
    ffrt::TaskFactory::Free(this);
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
    : parent(parent), rank(id), qos(qos)
{
    fq_we.task = this;
    if (attr && !attr->name_.empty()) {
        label = attr->name_;
    } else if (IsRoot()) {
        label = "root";
    } else if (parent->IsRoot()) {
        label = "t" + std::to_string(rank);
    } else {
        label = parent->label + "." + std::to_string(rank);
    }
    if (!IsRoot()) {
        FFRT_SUBMIT_MARKER(label, gid);
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
} /* namespace ffrt */