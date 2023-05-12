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

#include <memory>
#include "cpp/queue.h"
#include "cpp/task.h"
#include "core/task_attr_private.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/osal.h"
#include "sched/qos.h"
#include "serial_handler.h"
#include "serial_task.h"
#include "util/slab.h"

using namespace std;
using namespace ffrt;

namespace {
SerialTask* ffrt_queue_submit_base(ffrt_queue_t queue, ffrt_function_header_t* f, uint64_t delayUs = 0)
{
    FFRT_COND_TRUE_DO_ERR((queue == nullptr), "input invalid, queue == nullptr", return nullptr);
    FFRT_COND_TRUE_DO_ERR((f == nullptr), "input invalid, function header == nullptr", return nullptr);

    SerialTask* task = nullptr;
    {
        task = reinterpret_cast<SerialTask*>(static_cast<uintptr_t>(static_cast<size_t>(reinterpret_cast<uintptr_t>(f))
            - (reinterpret_cast<size_t>(&((reinterpret_cast<SerialTask*>(0))->func_storage)))));
    }

    SerialHandler* handler = static_cast<SerialHandler*>(queue);
    task->SetQueHandler(handler);
    handler->SubmitDelayed(task, delayUs);
    return task;
}
} // namespace

API_ATTRIBUTE((visibility("default")))
int ffrt_queue_attr_init(ffrt_queue_attr_t *attr)
{
    FFRT_COND_TRUE_DO_ERR((attr == nullptr), "input invalid, attr == nullptr", return -1);
    static_assert(sizeof(ffrt::task_attr_private) <= ffrt_task_attr_storage_size,
        "size must be less than ffrt_queue_attr_storage_size");

    new (attr)ffrt::task_attr_private();
    return 0;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_destroy(ffrt_queue_attr_t *attr)
{
    FFRT_COND_TRUE_DO_ERR((attr == nullptr), "input invalid, attr == nullptr", return);
    auto p = reinterpret_cast<ffrt::task_attr_private *>(attr);
    p->~task_attr_private();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_attr_set_qos(ffrt_queue_attr_t *attr, ffrt_qos_t qos)
{
    FFRT_COND_TRUE_DO_ERR((attr == nullptr), "input invalid, attr == nullptr", return);
    ffrt::QoS _qos = ffrt::QoS(qos);
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->qos_ = static_cast<ffrt::qos>(_qos());
}

API_ATTRIBUTE((visibility("default")))
ffrt_qos_t ffrt_queue_attr_get_qos(const ffrt_queue_attr_t *attr)
{
    FFRT_COND_TRUE_DO_ERR((attr == nullptr), "input invalid, attr == nullptr", return ffrt_qos_default);
    ffrt_queue_attr_t *p = const_cast<ffrt_queue_attr_t *>(attr);
    return static_cast<ffrt_qos_t>((reinterpret_cast<ffrt::task_attr_private *>(p))->qos_);
}

API_ATTRIBUTE((visibility("default")))
void *ffrt_alloc_auto_free_queue_func_storage_base()
{
    auto addr = ffrt::SimpleAllocator<SerialTask>::allocMem();
    new (addr)SerialTask();
    return addr->func_storage;
}

API_ATTRIBUTE((visibility("default")))
ffrt_queue_t ffrt_queue_create(ffrt_queue_type_t type, const char* name, const ffrt_queue_attr_t* attr)
{
    FFRT_COND_TRUE_DO_ERR((type != ffrt_queue_serial), "input invalid, type unsupport", return nullptr);
    FFRT_COND_TRUE_DO_ERR((attr == nullptr), "input invalid, attr == nullptr", return nullptr);

    ffrt::qos qos = static_cast<ffrt::qos>(ffrt_queue_attr_get_qos(attr));
    shared_ptr<SerialLooper> looper = make_shared<SerialLooper>(name, qos);
    FFRT_COND_TRUE_DO_ERR((looper == nullptr), "failed to construct SerialLooper", return nullptr);

    SerialHandler* handler = new SerialHandler(looper);
    FFRT_COND_TRUE_DO_ERR((handler == nullptr), "failed to construct SerialHandler", return nullptr);
    return static_cast<ffrt_queue_t>(handler);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_destroy(ffrt_queue_t queue)
{
    FFRT_COND_TRUE_DO_ERR((queue == nullptr), "input invalid, queue is nullptr", return);
    SerialHandler* handler = static_cast<SerialHandler*>(queue);
    delete handler;
    FFRT_LOGI("destroy queue succ");
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_submit(ffrt_queue_t queue, ffrt_function_header_t* f)
{
    SerialTask* task = ffrt_queue_submit_base(queue, f);
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "failed to submit serial task", return);
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_queue_submit_h(ffrt_queue_t queue, ffrt_function_header_t* f)
{
    SerialTask* task = ffrt_queue_submit_base(queue, f);
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "failed to submit serial task", return nullptr);
    task->IncDeleteRef();
    return static_cast<ffrt_task_handle_t>(task);
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_queue_submit_raw(
    ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr)
{
    SerialTask* task = ffrt_queue_submit_base(queue, f, ffrt_task_attr_get_delay(attr));
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "failed to submit serial task", return nullptr);
    task->IncDeleteRef();
    return static_cast<ffrt_task_handle_t>(task);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_destroy_task_handle(ffrt_task_handle_t handle)
{
    FFRT_COND_TRUE_DO_ERR((handle == nullptr), "input invalid, task_handle is nullptr", return);
    static_cast<SerialTask*>(handle)->DecDeleteRef();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_queue_wait(ffrt_task_handle_t handle)
{
    FFRT_COND_TRUE_DO_ERR((handle == nullptr), "input invalid, task_handle is nullptr", return);
    SerialTask* taskPtr = static_cast<SerialTask*>(handle);
    taskPtr->Wait();
    FFRT_LOGI("wait task [0x%x] succ", taskPtr);
}

API_ATTRIBUTE((visibility("default")))
int ffrt_queue_cancel(ffrt_task_handle_t handle)
{
    FFRT_COND_TRUE_DO_ERR((handle == nullptr), "input invalid, queue is nullptr", return -1);
    SerialTask* task = static_cast<SerialTask*>(handle);
    FFRT_COND_TRUE_DO_ERR((task->handler_ == nullptr), "input invalid, queue is nullptr", return -1);
    return task->handler_->Cancel(task);
}