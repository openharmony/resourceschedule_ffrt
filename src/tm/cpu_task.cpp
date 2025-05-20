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
#include "cpp/task_ext.h"
#include "util/ref_function_header.h"

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
    } else {
        this->qos_ = newQos;
    }
    FFRT_LOGD("Change task %s QoS %d", label.c_str(), this->qos_());
}

void CPUEUTask::Submit()
{
    status = TaskStatus::SUBMITTED;
    FFRTTraceRecord::TaskSubmit<ffrt_normal_task>(qos_, &createTime, &fromTid);
}

void CPUEUTask::Ready()
{
    int qos = qos_();
    bool notifyWorker = notifyWorker_;
    this->status = TaskStatus::READY;
    FFRTFacade::GetSchedInstance()->GetScheduler(this->qos_).PushTaskGlobal(this);
    FFRTTraceRecord::TaskEnqueue<ffrt_normal_task>(qos);
    if (notifyWorker) {
        FFRTFacade::GetEUInstance().NotifyTask<TaskNotifyType::TASK_ADDED>(qos);
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
    auto f = reinterpret_cast<ffrt_function_header_t*>(func_storage);
    if ((f->reserve[0] & MASK_FOR_HCS_TASK) != MASK_FOR_HCS_TASK) {
        if (f->destroy) {
            f->destroy(f);
        }
        TaskFactory<CPUEUTask>::Free(this);
        return;
    }
    FFRT_LOGD("hcs task deconstruct dec ref gid:%llu, create time:%llu", gid, createTime);
    this->~CPUEUTask();
    // hcs task dec ref
    reinterpret_cast<RefFunctionHeader*>(f->reserve[0] & (~MASK_FOR_HCS_TASK))->DecDeleteRef();
}

void CPUEUTask::Execute()
{
    FFRT_LOGD("Execute task[%lu], name[%s]", gid, label.c_str());
    FFRTTraceRecord::TaskExecute(&(this->executeTime));
    auto f = reinterpret_cast<ffrt_function_header_t*>(func_storage);
    auto exp = ffrt::SkipStatus::SUBMITTED;
    if (likely(__atomic_compare_exchange_n(&skipped, &exp, ffrt::SkipStatus::EXECUTED, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))) {
        status = TaskStatus::EXECUTING;
        f->exec(f);
    }
    FFRT_TASKDONE_MARKER(gid);
    // skipped task can not be marked as finish
    if (status == TaskStatus::EXECUTING) {
        status = TaskStatus::FINISH;
    }
    if (!USE_COROUTINE) {
        FFRTFacade::GetDMInstance().onTaskDone(this);
    } else {
        this->coRoutine->isTaskDone = true;
        /*
            if we call onTaskDone inside coroutine, the memory of task may be recycled.
            1.recycled memory can be used by another submit
            2.task->coRoutine will be recyled and used by another task
            In this scenario, CoStart will crash.
            Because it needs to use task and it's coRoutine to perform some action after task finished.
        */
    }
}

CPUEUTask::CPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id)
    : CoTask(ffrt_normal_task, attr), parent(parent), rank(id)
{
    if (attr && !attr->name_.empty()) {
        label = attr->name_;
    } else if (IsRoot()) {
        label = "root";
    } else if ((parent != nullptr) && parent->IsRoot()) {
        label = "t" + std::to_string(rank);
    } else if (parent != nullptr) {
        label = parent->label + "." + std::to_string(rank);
    }

    if (attr) {
        notifyWorker_ = attr->notifyWorker_;

        if (attr->qos_ == qos_inherit && !this->IsRoot()) {
            qos_ = parent->qos_;
        }
        if (attr->taskLocal_) {
            tsd = (void **)malloc(TSD_SIZE * sizeof(void *));
            if (unlikely(tsd == nullptr)) {
                FFRT_SYSEVENT_LOGE("task local malloc tsd failed");
                return;
            }
            memset_s(tsd, TSD_SIZE * sizeof(void *), 0, TSD_SIZE * sizeof(void *));
            taskLocal = true;
        }
    }
}
} /* namespace ffrt */