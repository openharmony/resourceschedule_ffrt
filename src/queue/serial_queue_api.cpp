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
#include "dm/dependence_manager.h"
#include "serial_handler.h"
#include "serial_task.h"
#include "util/event_handler_adapter.h"

using namespace std;
using namespace ffrt;

namespace {
inline void ResetTimeoutCb(ffrt::queue_attr_private* p)
{
    if (p->timeoutCb_ == nullptr) {
        return;
    }
    SerialTask* cbTask = GetSerialTaskByFuncStorageOffset(p->timeoutCb_);
    cbTask->DecDeleteRef();
    p->timeoutCb_ = nullptr;
}

inline SerialTask* ffrt_queue_submit_base(ffrt_queue_t queue, ffrt_function_header_t* f, bool withHandle,
    const ffrt_task_attr_t* attr)
{
    FFRT_COND_DO_ERR(unlikely(queue == nullptr), return nullptr, "input invalid, queue == nullptr");
    FFRT_COND_DO_ERR(unlikely(f == nullptr), return nullptr, "input invalid, function header == nullptr");
    SerialHandler* handler = static_cast<SerialHandler*>(queue);
    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    SerialTask* task = GetSerialTaskByFuncStorageOffset(f);
    new (task)ffrt::SerialTask(handler, p);
    if (withHandle) {
        task->IncDeleteRef();
    }

    handler->Submit(task);
    return task;
}
} // namespace

API_ATTRIBUTE((visibility("default")))
int ffrt_queue_attr_init(ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return -1, "input invalid, attr == nullptr");
    static_assert(sizeof(ffrt::queue_attr_private) <= ffrt_queue_attr_storage_size,
        "size must be less than ffrt_queue_attr_storage_size");

    new (attr) ffrt::queue_attr_private();
    return 0;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_destroy(ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");
    auto p = reinterpret_cast<ffrt::queue_attr_private*>(attr);
    ResetTimeoutCb(p);
    p->~queue_attr_private();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_qos(ffrt_queue_attr_t* attr, ffrt_qos_t qos)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");

    (reinterpret_cast<ffrt::queue_attr_private*>(attr))->qos_ = ffrt::GetFuncQosMap()(qos);
}

API_ATTRIBUTE((visibility("default")))
ffrt_qos_t ffrt_queue_attr_get_qos(const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return ffrt_qos_default, "input invalid, attr == nullptr");
    ffrt_queue_attr_t* p = const_cast<ffrt_queue_attr_t*>(attr);
    return (reinterpret_cast<ffrt::queue_attr_private*>(p))->qos_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_timeout(ffrt_queue_attr_t* attr, uint64_t timeout_us)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");
    (reinterpret_cast<ffrt::queue_attr_private*>(attr))->timeout_ = timeout_us;
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_queue_attr_get_timeout(const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return 0, "input invalid, attr == nullptr");
    ffrt_queue_attr_t* p = const_cast<ffrt_queue_attr_t*>(attr);
    return (reinterpret_cast<ffrt::queue_attr_private*>(p))->timeout_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_callback(ffrt_queue_attr_t* attr, ffrt_function_header_t* f)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");
    ffrt::queue_attr_private* p = reinterpret_cast<ffrt::queue_attr_private*>(attr);
    ResetTimeoutCb(p);
    p->timeoutCb_ = f;
    // the memory of timeoutCb are managed in the form of SerialTask
    SerialTask* task = GetSerialTaskByFuncStorageOffset(f);
    new (task)ffrt::SerialTask(nullptr);
}

API_ATTRIBUTE((visibility("default")))
ffrt_function_header_t* ffrt_queue_attr_get_callback(const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return nullptr, "input invalid, attr == nullptr");
    ffrt_queue_attr_t* p = const_cast<ffrt_queue_attr_t*>(attr);
    return (reinterpret_cast<ffrt::queue_attr_private*>(p))->timeoutCb_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_max_concurrency(ffrt_queue_attr_t* attr, const int max_concurrency)
{
    FFRT_COND_DO_ERR((attr == nullptr), return, "input invalid, attr == nullptr");

    FFRT_COND_DO_ERR((max_concurrency <= 0), return,
        "max concurrency should be a valid value");

    (reinterpret_cast<ffrt::queue_attr_private*>(attr))->maxConcurrency_ = max_concurrency;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_queue_attr_get_max_concurrency(const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR((attr == nullptr), return 0, "input invalid, attr == nullptr");
    ffrt_queue_attr_t* p = const_cast<ffrt_queue_attr_t*>(attr);
    return (reinterpret_cast<ffrt::queue_attr_private*>(p))->maxConcurrency_;
}

API_ATTRIBUTE((visibility("default")))
ffrt_queue_t ffrt_queue_create(ffrt_queue_type_t type, const char* name, const ffrt_queue_attr_t* attr)
{
    FFRT_COND_DO_ERR(((type != ffrt_queue_serial) && (type != ffrt_queue_concurrent)),
        return nullptr, "input invalid, type unsupport");
    SerialHandler* handler = new (std::nothrow) SerialHandler(name, attr, type);
    FFRT_COND_DO_ERR((handler == nullptr), return nullptr, "failed to construct SerialHandler");
    handler->SetHandlerType(NORMAL_SERIAL_HANDLER);
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
    FFRT_COND_DO_ERR((f == nullptr), return, "input invalid, function is nullptr");
    SerialTask* task = ffrt_queue_submit_base(queue, f, false, attr);
    FFRT_COND_DO_ERR((task == nullptr), return, "failed to submit serial task");
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_queue_submit_h(ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr)
{
    FFRT_COND_DO_ERR((f == nullptr), return nullptr, "input invalid, function is nullptr");
    SerialTask* task = ffrt_queue_submit_base(queue, f, true, attr);
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
    SerialTask* task = reinterpret_cast<SerialTask*>(static_cast<CPUEUTask*>(handle));
    IHandler* handler = task->GetHandler();
    FFRT_COND_DO_ERR((handler == nullptr), return -1, "task handler is nullptr");
    int ret = handler->Cancel(task);
    return ret;
}

#ifdef OHOS_STANDARD_SYSTEM
API_ATTRIBUTE((visibility("default")))
ffrt_queue_t ffrt_get_main_queue()
{
    void* mainHandler = EventHandlerAdapter::Instance()->GetMainEventHandler();
    FFRT_COND_DO_ERR((mainHandler == nullptr), return nullptr, "failed to get main queue.");
    SerialHandler *handler = new (std::nothrow) SerialHandler("main_queue", nullptr);
    FFRT_COND_DO_ERR((handler == nullptr), return nullptr, "failed to construct MainThreadSerialHandler");
    handler->SetHandlerType(MAINTHREAD_SERIAL_HANDLER);
    handler->SetEventHandler(mainHandler);
    return static_cast<ffrt_queue_t>(handler);
}

API_ATTRIBUTE((visibility("default")))
ffrt_queue_t ffrt_get_current_queue()
{
    void* workerHandler = EventHandlerAdapter::Instance()->GetCurrentEventHandler();
    FFRT_COND_DO_ERR((workerHandler == nullptr), return nullptr, "failed to get ArkTs worker queue.");
    SerialHandler *handler = new (std::nothrow) SerialHandler("current_queue", nullptr);
    FFRT_COND_DO_ERR((handler == nullptr), return nullptr, "failed to construct WorkerThreadSerialHandler");
    handler->SetHandlerType(WORKERTHREAD_SERIAL_HANDLER);
    handler->SetEventHandler(workerHandler);
    return static_cast<ffrt_queue_t>(handler);
}
#endif