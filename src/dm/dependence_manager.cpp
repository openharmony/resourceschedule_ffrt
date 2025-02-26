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

#include "dependence_manager.h"
#include "util/ffrt_facade.h"
#include "util/singleton_register.h"
#include "tm/io_task.h"

namespace ffrt {
DependenceManager& DependenceManager::Instance()
{
    return SingletonRegister<DependenceManager>::Instance();
}

void DependenceManager::RegistInsCb(SingleInsCB<DependenceManager>::Instance &&cb)
{
    SingletonRegister<DependenceManager>::RegistInsCb(std::move(cb));
}

void DependenceManager::onSubmitUV(ffrt_executor_task_t *task, const task_attr_private *attr)
{
    FFRT_EXECUTOR_TASK_SUBMIT_MARKER(task);
    FFRT_TRACE_SCOPE(1, onSubmitUV);
    QoS qos = (attr == nullptr || attr->qos_ == qos_inherit) ? QoS() : QoS(attr->qos_);
    FFRTTraceRecord::TaskSubmit<ffrt_uv_task>(qos);
    LinkedList* node = reinterpret_cast<LinkedList *>(&task->wq);
    FFRTScheduler* sch = FFRTFacade::GetSchedInstance();
    if (!sch->InsertNode(node, qos)) {
        FFRT_LOGE("Submit UV task failed!");
        return;
    }
    FFRTTraceRecord::TaskEnqueue<ffrt_uv_task>(qos);
}

void DependenceManager::onSubmitIO(const ffrt_io_callable_t& work, const task_attr_private* attr)
{
    FFRT_TRACE_SCOPE(1, onSubmitIO);
    IOTask* ioTask = TaskFactory<IOTask>::Alloc();
    new (ioTask) IOTask(work, attr);
    FFRTTraceRecord::TaskSubmit<ffrt_io_task>(ioTask->qos_);
    if (!FFRTFacade::GetSchedInstance()->InsertNode(&ioTask->fq_we.node, ioTask->qos_)) {
        FFRT_LOGE("Submit IO task failed!");
        return;
    }
    FFRTTraceRecord::TaskEnqueue<ffrt_io_task>(ioTask->qos_);
}
}