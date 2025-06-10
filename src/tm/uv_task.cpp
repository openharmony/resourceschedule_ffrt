/*
* Copyright (c) 2024 Huawei Device Co., Ltd.
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

#include "uv_task.h"
#include "eu/func_manager.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "dfx/trace/ffrt_trace.h"
#include "util/ffrt_facade.h"

namespace ffrt {
void UVTask::Ready()
{
    QoS taskQos = qos_;
    FFRTTraceRecord::TaskSubmit<ffrt_uv_task>(taskQos);
    SetTaskStatus(TaskStatus::READY);
    FFRTFacade::GetSchedInstance()->GetScheduler(taskQos).PushTaskGlobal(this);
    FFRTTraceRecord::TaskEnqueue<ffrt_uv_task>(taskQos);
    FFRTFacade::GetEUInstance().NotifyTask<TaskNotifyType::TASK_ADDED>(taskQos);
}

void UVTask::Execute()
{
    if (uvWork == nullptr) {
        FFRT_SYSEVENT_LOGE("task is nullptr");
        DecDeleteRef();
        return;
    }
    ffrt_executor_task_func func = FuncManager::Instance()->getFunc(ffrt_uv_task);
    if (func == nullptr) {
        FFRT_SYSEVENT_LOGE("Static func is nullptr");
        DecDeleteRef();
        return;
    }
    FFRTTraceRecord::TaskExecute<ffrt_uv_task>(qos_);
    FFRT_EXECUTOR_TASK_BEGIN(uvWork);
    func(uvWork, qos_);
    FFRT_EXECUTOR_TASK_END();
    FFRT_EXECUTOR_TASK_FINISH_MARKER(uvWork); // task finish maker for uv task
    FFRTTraceRecord::TaskDone<ffrt_uv_task>(qos_);
    DecDeleteRef();
}
} // namespace ffrt
