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
#include "serial_handler.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
int SerialHandler::Cancel(ITask* task)
{
    FFRT_COND_DO_ERR((task == nullptr), return -1, "submit task is nullptr");
    FFRT_COND_DO_ERR((looper_ == nullptr || looper_->queue_ == nullptr), return -1, "queue is nullptr");
    int ret = looper_->queue_->RemoveTask(task);
    FFRT_LOGD("cancel serial task [%p] return [%d]", task, ret);
    if (ret == 0) {
	auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
	f->destroy(f);
        DestroyTask(task);
    }
    return ret;
}

void SerialHandler::DispatchTask(ITask* task)
{
    FFRT_COND_DO_ERR((task == nullptr), return, "failed to dispatch, task is nullptr");
    auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
    f->exec(f);
    f->destroy(f);
    DestroyTask(task);
    FFRT_LOGD("dispatch serial task [%p] succ", task);
}

int SerialHandler::SubmitDelayed(ITask* task, uint64_t delayUs)
{
    FFRT_COND_DO_ERR((task == nullptr), return -1, "submit task is nullptr");
    FFRT_COND_DO_ERR((looper_ == nullptr || looper_->queue_ == nullptr), return -1, "queue is nullptr");
    FFRT_LOGD("submit serial task [%p] with delay [%llu us]", task, delayUs);
    auto nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    uint64_t upTime = static_cast<uint64_t>(nowUs.time_since_epoch().count());
    if (delayUs > 0) {
        upTime = upTime + delayUs;
    }
    return looper_->queue_->PushTask(task, upTime);
}

void SerialHandler::DestroyTask(ITask* task)
{
    task->Notify();
    task->DecDeleteRef();
}
} // namespace ffrt
