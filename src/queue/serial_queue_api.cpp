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

#include "cpp/queue.h"
#include "core/dependence_manager.h"
#include "serial_handler.h"
#include "serial_task.h"

using namespace std;
using namespace ffrt;

namespace {
inline void ResetTimeoutCb(ffrt::task_attr_private* p)
{
    if (p->timeoutCb_ == nullptr) {
        return;
    }
    GetSerialTaskByFuncStorageOffset(p->timeoutCb_)->DecDeleteRef();
    p->timeoutCb_ = nullptr;
}

inline SerialTask* ffrt_queue_submit_base(ffrt_queue_t queue, ffrt_function_header_t* f, bool withHandle,
    uint64_t& delayUs)
{
    FFRT_COND_DO_ERR((queue == nullptr), return nullptr, "input invalid, queue == nullptr");
    FFRT_COND_DO_ERR((f == nullptr), return nullptr, "input invalid, function header == nullptr");

    SerialTask* task = GetSerialTaskByFuncStorageOffset(f);
    new (task)ffrt::SerialTask();
    SerialHandler* handler = static_cast<SerialHandler*>(queue);
    task->SetQueHandler(handler);

    if (withHandle) {
        task->IncDeleteRef();
    }
    handler->SubmitDelayed(task, delayUs);
    return task;
}
} // namespace

API_ATTRIBUTE((visibility("default")))
int ffrt_queue_attr_init(ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return -1, "input invalid, attr == nullptr");
    static_assert(sizeof(ffrt::task_attr_private) <= ffrt_task_attr_storage_size,
        "size must be less than ffrt_queue_attr_storage_size");

    new (attr) ffrt::task_attr_private();
    return 0;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_destroy(ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");
    auto p = reinterpret_cast<ffrt::task_attr_private*>(attr);
    ResetTimeoutCb(p);
    p->~task_attr_private();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_qos(ffrt_queue_attr_t* attr, ffrt_qos_t qos)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");
    (reinterpret_cast<ffrt::task_attr_private*>(attr))->qos_map = qos;
}

API_ATTRIBUTE((visibility("default")))
ffrt_qos_t ffrt_queue_attr_get_qos(const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return ffrt_qos_default, "input invalid, attr == nullptr");
    ffrt_queue_attr_t* p = const_cast<ffrt_queue_attr_t*>(attr);
    return (reinterpret_cast<ffrt::task_attr_private*>(p))->qos_map.m_qos;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_timeout(ffrt_queue_attr_t* attr, uint64_t timeout_us)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");
    (reinterpret_cast<ffrt::task_attr_private*>(attr))->timeout_ = timeout_us;
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_queue_attr_get_timeout(const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return 0, "input invalid, attr == nullptr");
    ffrt_queue_attr_t* p = const_cast<ffrt_queue_attr_t*>(attr);
    return (reinterpret_cast<ffrt::task_attr_private*>(p))->timeout_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_callback(ffrt_queue_attr_t* attr, ffrt_function_header_t* f)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");
    ffrt::task_attr_private* p = reinterpret_cast<ffrt::task_attr_private*>(attr);
    ResetTimeoutCb(p);
    p->timeoutCb_ = f;
    // the memory of timeoutCb are managed in the form of SerialTask
    SerialTask* task = GetSerialTaskByFuncStorageOffset(f);
    new (task)ffrt::SerialTask();
}

API_ATTRIBUTE((visibility("default")))
ffrt_function_header_t* ffrt_queue_attr_get_callback(const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return nullptr, "input invalid, attr == nullptr");
    ffrt_queue_attr_t* p = const_cast<ffrt_queue_attr_t*>(attr);
    return (reinterpret_cast<ffrt::task_attr_private*>(p))->timeoutCb_;
}

API_ATTRIBUTE((visibility("default")))
ffrt_queue_t ffrt_queue_create(ffrt_queue_type_t type, const char* name, const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((type != ffrt_queue_serial), return nullptr, "input invalid, type unsupport");
    FFRT_COND_DO_ERR((attr == nullptr), return nullptr, "input invalid, attr == nullptr");

    int qos = ffrt_queue_attr_get_qos(attr);
    shared_ptr<SerialLooper> looper =
        make_shared<SerialLooper>(name, qos, ffrt_queue_attr_get_timeout(attr), ffrt_queue_attr_get_callback(attr));
    FFRT_COND_DO_ERR((looper == nullptr), return nullptr, "failed to construct SerialLooper");

    SerialHandler* handler = new (std::nothrow) SerialHandler(looper);
    FFRT_COND_DO_ERR((handler == nullptr), return nullptr, "failed to construct SerialHandler");
    return static_cast<ffrt_queue_t>(handler);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_destroy(ffrt_queue_t queue)
{
    FFRT_COND_DO_ERR((queue == nullptr), return, "input invalid, queue is nullptr");
    SerialHandler* handler = static_cast<SerialHandler*>(queue);
    delete handler;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_submit(ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr)
{
    uint64_t delayUs = (attr == nullptr) ? 0 : ffrt_task_attr_get_delay(attr);
    SerialTask* task = ffrt_queue_submit_base(queue, f, false, delayUs);
    FFRT_COND_DO_ERR((task == nullptr), return, "failed to submit serial task");
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_queue_submit_h(ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr)
{
    uint64_t delayUs = (attr == nullptr) ? 0 : ffrt_task_attr_get_delay(attr);
    SerialTask* task = ffrt_queue_submit_base(queue, f, true, delayUs);
    FFRT_COND_DO_ERR((task == nullptr), return nullptr, "failed to submit serial task");
    return static_cast<ffrt_task_handle_t>(task);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_wait(ffrt_task_handle_t handle)
{
    FFRT_COND_DO_ERR((handle == nullptr), return, "input invalid, task_handle is nullptr");
    SerialTask* task = static_cast<SerialTask*>(handle);
    task->Wait();
}

API_ATTRIBUTE((visibility("default")))
int ffrt_queue_cancel(ffrt_task_handle_t handle)
{
    FFRT_COND_DO_ERR((handle == nullptr), return -1, "input invalid, handle is nullptr");
    SerialTask* task = static_cast<SerialTask*>(handle);
    FFRT_COND_DO_ERR((task->handler_ == nullptr), return -1, "input invalid, task->handler_ is nullptr");
    return task->handler_->Cancel(task);
}