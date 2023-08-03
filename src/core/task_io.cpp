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
#include <pthread.h>
#include <random>
#include <ffrt.h>
#include "cpp/task.h"
#include "c/task.h"
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
#include <dlfcn.h>
#include "libunwind.h"
#endif
#include "core/dependence_manager.h"
#include "core/task_io.h"
#include "util/slab.h"
#include "internal_inc/osal.h"
#include "queue/queue.h"

#define ENABLE_LOCAL_QUEUE

namespace ffrt {
static inline void ffrt_exec_callable_wrapper(void* t)
{
    ffrt::ffrt_io_callable_t* f = (ffrt::ffrt_io_callable_t*)t;
    f->exec(f->callable);
}

static inline void ffrt_destroy_callable_wrapper(void* t)
{
    ffrt::ffrt_io_callable_t* f = (ffrt::ffrt_io_callable_t*)t;
    f->destroy(f->callable);
}

static void exec_wake_callable(ffrt_executor_io_task* task)
{
    task->lock.lock();
    task->status = ExecTaskStatus::ET_FINISH;
    task->lock.unlock();
    if (task->wakeFlag&&task->wake_callable_on_finish.exec) {
        ffrt_exec_callable_wrapper((void*)&(task->wake_callable_on_finish));
    }
    if (task->wakeFlag&&task->wake_callable_on_finish.destroy) {
        ffrt_destroy_callable_wrapper((void*)&(task->wake_callable_on_finish));
    }
    auto f = (ffrt_function_header_t*)task->func_storage;
    f->destroy(f);
    task->freeMem();
}

static void io_ffrt_executor_task_func(ffrt_executor_task_t* data)
{
    ffrt_executor_io_task* task = static_cast<ffrt_executor_io_task*>(data);
    __atomic_store_n(&task->status, ExecTaskStatus::ET_EXECUTING, __ATOMIC_SEQ_CST);
    auto f = (ffrt_function_header_t*)task->func_storage;
    ffrt_coroutine_ptr_t coroutine = (ffrt_coroutine_ptr_t)f->exec;
    ffrt_coroutine_ret_t ret = coroutine(f);
    if (ret == ffrt_coroutine_ready) {
        exec_wake_callable(task);
        return;
    }
    ExecTaskStatus executing_status = ExecTaskStatus::ET_EXECUTING;
    ExecTaskStatus toready_status = ExecTaskStatus::ET_TOREADY;
    if (__atomic_compare_exchange_n(&task->status, &executing_status, ExecTaskStatus::ET_PENDING, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return;
    }
    if (likely(__atomic_compare_exchange_n(&task->status, &toready_status, ExecTaskStatus::ET_READY, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))) {
#ifdef ENABLE_LOCAL_QUEUE
        if (ffrt::ExecuteCtx::Cur()->PushTaskToPriorityStack(task)) return;
        if (ffrt::ExecuteCtx::Cur()->local_fifo == nullptr ||
            queue_pushtail(ffrt::ExecuteCtx::Cur()->local_fifo, task) == ERROR_QUEUE_FULL) {
            LinkedList* node = (LinkedList *)(&task->wq);
            if (!FFRTScheduler::Instance()->InsertNode(node, task->qos)) {
                FFRT_LOGE("Submit IO task failed");
            }
            return;
        }
        ffrt::ExecuteUnit::Instance().NotifyLocalTaskAdded(task->qos);
#else
        LinkedList* node = (LinkedList *)(&task->wq);
        if (!FFRTScheduler::Instance()->InsertNode(node, task->qos)) {
            FFRT_LOGE("Submit IO task failed");
        }
#endif
    }
}

static pthread_once_t once = PTHREAD_ONCE_INIT;

static void ffrt_executor_io_task_init()
{
    ffrt_executor_task_register_func_(io_ffrt_executor_task_func, ffrt_io_task);
}

bool randomBool()
{
    static auto gen = std::bind(std::uniform_int_distribution<>(0, 1), std::default_random_engine());
    return gen();
}
}

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ffrt_function_header_t header;
    ffrt_coroutine_ptr_t func;
    ffrt_function_ptr_t destroy;
    void* arg;
} ffrt_function_coroutine_t;

static ffrt_coroutine_ret_t ffrt_exec_function_coroutine_wrapper(void* t)
{
    ffrt_function_coroutine_t* f = (ffrt_function_coroutine_t*)t;
    return f->func(f->arg);
}

static void ffrt_destory_function_coroutine_wrapper(void* t)
{
    ffrt_function_coroutine_t* f = (ffrt_function_coroutine_t*)t;
    f->destroy(f->arg);
}

static inline ffrt_function_header_t* ffrt_create_function_coroutine_wrapper(void* co,
    ffrt_coroutine_ptr_t exec, ffrt_function_ptr_t destroy)
{
    static_assert(sizeof(ffrt_function_coroutine_t) <= ffrt_auto_managed_function_storage_size,
        "size_of_ffrt_function_coroutine_t_must_be_less_than_ffrt_auto_managed_function_storage_size");
    ffrt_function_coroutine_t* f = (ffrt_function_coroutine_t*)ffrt_alloc_auto_managed_function_storage_base(
        ffrt_function_kind_io);
    f->header.exec = (ffrt_function_ptr_t)ffrt_exec_function_coroutine_wrapper;
    f->header.destroy = ffrt_destory_function_coroutine_wrapper;
    f->func = exec;
    f->destroy = destroy;
    f->arg = co;
    return (ffrt_function_header_t*) f;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_submit_coroutine(void* co, ffrt_coroutine_ptr_t exec, ffrt_function_ptr_t destroy,
    const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr)
{
    pthread_once(&ffrt::once, ffrt::ffrt_executor_io_task_init);
    ffrt_function_header_t* f = ffrt_create_function_coroutine_wrapper(co, exec, destroy);
    if (unlikely(!f)) {
        FFRT_LOGE("function handler should not be empty");
        return;
    }

    ffrt::ffrt_executor_io_task* task = nullptr;
    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    ffrt::QoS qos = (p == nullptr ? ffrt::QoS() : ffrt::QoS(p->qos_));
    {
        task = reinterpret_cast<ffrt::ffrt_executor_io_task*>(static_cast<uintptr_t>(
            static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) - OFFSETOF(ffrt::ffrt_executor_io_task, func_storage)));
        new (task)ffrt::ffrt_executor_io_task(qos);
    }
    ffrt::ExecTaskStatus pending_status = ffrt::ExecTaskStatus::ET_PENDING;
    if (likely(__atomic_compare_exchange_n(&task->status, &pending_status, ffrt::ExecTaskStatus::ET_READY, 0,
    __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))) {
        ffrt::DependenceManager::Instance()->onSubmitUV(task, p);
    }
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_submit_h_coroutine(void* co, ffrt_coroutine_ptr_t exec,
    ffrt_function_ptr_t destroy, const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr)
{
    pthread_once(&ffrt::once, ffrt::ffrt_executor_io_task_init);
    ffrt_function_header_t* f = ffrt_create_function_coroutine_wrapper(co, exec, destroy);
    if (unlikely(!f)) {
        FFRT_LOGE("function handler should not be empty");
        return nullptr;
    }

    ffrt::ffrt_executor_io_task* task = nullptr;
    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    ffrt::QoS qos = (p == nullptr ? ffrt::QoS() : ffrt::QoS(p->qos_));
    {
        task = reinterpret_cast<ffrt::ffrt_executor_io_task*>(static_cast<uintptr_t>(
            static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) - OFFSETOF(ffrt::ffrt_executor_io_task, func_storage)));
        new (task)ffrt::ffrt_executor_io_task(qos);
    }
    task->status = ffrt::ExecTaskStatus::ET_READY;
    ffrt::DependenceManager::Instance()->onSubmitUV(task, p);
    return task;
}

// waker
API_ATTRIBUTE((visibility("default")))
void ffrt_wake_by_handle(void* callable, ffrt_function_ptr_t exec, ffrt_function_ptr_t destroy,
    ffrt_task_handle_t handle)
{
    ffrt::ffrt_executor_io_task* task = static_cast<ffrt::ffrt_executor_io_task*>(handle);
    task->lock.lock();
    FFRT_LOGD("tid:%ld ffrt_wake_by_handle and CurState = %d", syscall(SYS_gettid), task->status);
    if (task->status != ffrt::ExecTaskStatus::ET_FINISH) {
        task->wake_callable_on_finish.callable = callable;
        task->wake_callable_on_finish.exec = exec;
        task->wake_callable_on_finish.destroy = destroy;
    }
    task->lock.unlock();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_set_wake_flag(bool flag)
{
    ffrt::ffrt_executor_io_task* task = static_cast<ffrt::ffrt_executor_io_task*>(ffrt_task_get());
    task->SetWakeFlag(flag);
}

API_ATTRIBUTE((visibility("default")))
void * ffrt_task_get()
{
    return (void*)ffrt::ExecuteCtx::Cur()->task;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_wake_coroutine(void *taskin)
{
    ffrt::ffrt_executor_io_task* task = static_cast<ffrt::ffrt_executor_io_task*>(taskin);
    ffrt::ExecTaskStatus executing_status = ffrt::ExecTaskStatus::ET_EXECUTING;
    ffrt::ExecTaskStatus pending_status = ffrt::ExecTaskStatus::ET_PENDING;
    FFRT_LOGD("ffrt wake loop %d", task->status);
    if (__atomic_compare_exchange_n(&task->status, &executing_status, ffrt::ExecTaskStatus::ET_TOREADY, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
        return;
    }
    if (__atomic_compare_exchange_n(&task->status, &pending_status, ffrt::ExecTaskStatus::ET_READY, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
#ifdef ENABLE_LOCAL_QUEUE
        if (ffrt::ExecuteCtx::Cur()->PushTaskToPriorityStack(task)) return;
        if (rand()%5) {
            if (ffrt::ExecuteCtx::Cur()->local_fifo != nullptr &&
                queue_pushtail(ffrt::ExecuteCtx::Cur()->local_fifo, task) != ERROR_QUEUE_FULL) {
                ffrt::ExecuteUnit::Instance().NotifyLocalTaskAdded(task->qos);
                return;
            }
        }
        ffrt::LinkedList* node = (ffrt::LinkedList *)(&task->wq);
        if (!ffrt::FFRTScheduler::Instance()->InsertNode(node, task->qos)) {
            FFRT_LOGE("Submit IO task failed");
        }
#else
        ffrt::LinkedList* node = (ffrt::LinkedList *)(&task->wq);
        if (!ffrt::FFRTScheduler::Instance()->InsertNode(node, task->qos)) {
            FFRT_LOGE("Submit IO task failed");
        }
#endif
    }
}

#ifdef __cplusplus
}
#endif