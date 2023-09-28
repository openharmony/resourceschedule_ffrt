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
#include "ffrt_inner.h"
#include "core/task_io.h"
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
#include <dlfcn.h>
#include "libunwind.h"
#endif
#include "core/dependence_manager.h"
#include "util/slab.h"
#ifdef FFRT_IO_TASK_SCHEDULER
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
    // 本次执行结束时stackless coroutine执行结束
    // 当前task执行完毕时，唤醒wait其执行结果的父任务
    if (task->wakeFlag && task->wake_callable_on_finish.exec) {
        ffrt_exec_callable_wrapper((void*)&(task->wake_callable_on_finish));
    }
    if (task->wakeFlag && task->wake_callable_on_finish.destroy) {
        ffrt_destroy_callable_wrapper((void*)&(task->wake_callable_on_finish));
    }
    // stackless coroutine对象本身释放
    auto f = (ffrt_function_header_t*)task->func_storage;
    f->destroy(f);

#ifdef FFRT_BBOX_ENABLE
    TaskDoneCounterInc();
#endif
    if (task->withHandle == false) {
        task->freeMem();
    }
}

static void io_ffrt_executor_task_func(ffrt_executor_task_t* data, ffrt_qos_t qos)
{
    ffrt_executor_io_task* task = static_cast<ffrt_executor_io_task*>(data);
    task->lock.lock();
    __atomic_store_n(&task->status, ExecTaskStatus::ET_EXECUTING, __ATOMIC_SEQ_CST);
    task->lock.unlock();
    auto f = (ffrt_function_header_t*)task->func_storage;
    ffrt_coroutine_ptr_t coroutine = (ffrt_coroutine_ptr_t)f->exec;
    ffrt_coroutine_ret_t ret = coroutine(f);
    if (ret == ffrt_coroutine_ready) {
        exec_wake_callable(task);
        return;
    }
    ExecTaskStatus executing_status = ExecTaskStatus::ET_EXECUTING;
    ExecTaskStatus toready_status = ExecTaskStatus::ET_TOREADY;
    std::lock_guard lg(task->lock);
    if (__atomic_compare_exchange_n(&task->status, &executing_status, ExecTaskStatus::ET_PENDING, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
        return;
    }
    if (likely(__atomic_compare_exchange_n(&task->status, &toready_status, ExecTaskStatus::ET_READY, 0,
        __ATOMIC_SEQ_CST, __ATOMIC_RELAXED))) {
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
#ifdef ENABLE_LOCAL_QUEUE
        if (ffrt::ExecuteCtx::Cur()->PushTaskToPriorityStack(task)) return;
        if (ffrt::ExecuteCtx::Cur()->local_fifo == nullptr ||
            queue_pushtail(ffrt::ExecuteCtx::Cur()->local_fifo, task) == ERROR_QUEUE_FULL) {
            LinkedList* node = (LinkedList *)(&task->wq);
            if (!FFRTScheduler::Instance()->InsertNode(node, task->qos)) {
                FFRT_LOGE("Submit io task failed!");
            }
            return;
        }
        ffrt::ExecuteUnit::Instance().NotifyLocalTaskAdded(task->qos);
#else
        LinkedList* node = (LinkedList *)(&task->wq);
        if (!FFRTScheduler::Instance()->InsertNode(node, task->qos)) {
            FFRT_LOGE("Submit io task failed!");
        }
#endif
    }
}

static pthread_once_t once = PTHREAD_ONCE_INIT;

static void ffrt_executor_io_task_init()
{
    ffrt_executor_task_register_func(io_ffrt_executor_task_func, ffrt_io_task);
}
} /* namespace ffrt */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ffrt_function_header_t header;
    ffrt_coroutine_ptr_t func;
    ffrt_function_t destroy;
    void* arg;
} ffrt_function_coroutine_t;

static ffrt_coroutine_ret_t ffrt_exec_function_coroutine_wrapper(void* t)
{
    ffrt_function_coroutine_t* f = (ffrt_function_coroutine_t*)t;
    return f->func(f->arg);
}

static void ffrt_destroy_function_coroutine_wrapper(void* t)
{
    ffrt_function_coroutine_t* f = (ffrt_function_coroutine_t*)t;
    f->destroy(f->arg);
}

static inline ffrt_function_header_t* ffrt_create_function_coroutine_wrapper(void* co, ffrt_coroutine_ptr_t exec,
    ffrt_function_t destroy)
{
    static_assert(sizeof(ffrt_function_coroutine_t) <= ffrt_auto_managed_function_storage_size,
        "size_of_ffrt_function_coroutine_t_must_be_less_than_ffrt_auto_managed_function_storage_size");
    FFRT_COND_DO_ERR((co == nullptr), return nullptr, "input invalid, co == nullptr");
    ffrt_function_coroutine_t* f = (ffrt_function_coroutine_t*)
        ffrt_alloc_auto_managed_function_storage_base(ffrt_function_kind_io);
    f->header.exec = (ffrt_function_t)ffrt_exec_function_coroutine_wrapper;
    f->header.destroy = ffrt_destroy_function_coroutine_wrapper;
    f->func = exec;
    f->destroy = destroy;
    f->arg = co;
    return (ffrt_function_header_t*)f;
}

static inline ffrt::ffrt_executor_io_task* prepare_task(ffrt_function_header_t* f,
    ffrt::QoS qos, bool withHandle)
{
    ffrt::ffrt_executor_io_task* task = nullptr;
    task = reinterpret_cast<ffrt::ffrt_executor_io_task*>(static_cast<uintptr_t>(
        static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) - OFFSETOF(ffrt::ffrt_executor_io_task, func_storage)));
    new (task)ffrt::ffrt_executor_io_task(qos);
    task->status = ffrt::ExecTaskStatus::ET_READY;
    task->withHandle = withHandle;
    return task;
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_submit_h_coroutine(void* co, ffrt_coroutine_ptr_t exec, ffrt_function_t destroy,
    const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr)
{
    pthread_once(&ffrt::once, ffrt::ffrt_executor_io_task_init);
    ffrt_function_header_t* f = ffrt_create_function_coroutine_wrapper(co, exec, destroy);
    if (unlikely(!f)) {
        FFRT_LOGE("function handler should not be empty");
        return nullptr;
    }

    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    ffrt::QoS qos = (p == nullptr ? ffrt::QoS() : ffrt::QoS(p->qos_));
    ffrt::ffrt_executor_io_task* task = prepare_task(f, qos, true);
    ffrt::DependenceManager::Instance()->onSubmitUV(task, p);
    return task;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_submit_coroutine(void* co, ffrt_coroutine_ptr_t exec, ffrt_function_t destroy, const ffrt_deps_t* in_deps,
    const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr)
{
    pthread_once(&ffrt::once, ffrt::ffrt_executor_io_task_init);
    ffrt_function_header_t* f = ffrt_create_function_coroutine_wrapper(co, exec, destroy);
    if (unlikely(!f)) {
        FFRT_LOGE("function handler should not be empty");
        return;
    }

    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    ffrt::QoS qos = (p == nullptr ? ffrt::QoS() : ffrt::QoS(p->qos_));
    ffrt::ffrt_executor_io_task* task = prepare_task(f, qos, false);
    ffrt::DependenceManager::Instance()->onSubmitUV(task, p);
}

// waker
API_ATTRIBUTE((visibility("default")))
void ffrt_wake_by_handle(void* callable, ffrt_function_t exec, ffrt_function_t destroy,
    ffrt_task_handle_t handle)
{
    FFRT_COND_DO_ERR((callable == nullptr), return, "input invalid, callable == nullptr");
    FFRT_COND_DO_ERR((handle == nullptr), return, "input invalid, handle == nullptr");
    ffrt::ffrt_executor_io_task* task = reinterpret_cast<ffrt::ffrt_executor_io_task*>(handle);
    task->lock.lock();
    FFRT_LOGD("tid: %ld ffrt_wake_by_handle and CurState = %d", syscall(SYS_gettid), task->status);
    if (task->status != ffrt::ExecTaskStatus::ET_FINISH) {
        task->wake_callable_on_finish.callable = callable;
        task->wake_callable_on_finish.exec = exec;
        task->wake_callable_on_finish.destroy = destroy;
    }
    task->lock.unlock();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_set_wake_flag(int flag)
{
    ffrt::ffrt_executor_io_task* task = reinterpret_cast<ffrt::ffrt_executor_io_task*>(ffrt_task_get());
    task->SetWakeFlag(flag);
}

API_ATTRIBUTE((visibility("default")))
void *ffrt_task_get()
{
    return (void *)ffrt::ExecuteCtx::Cur()->exec_task;
}

// API used to schedule stackless coroutine task
API_ATTRIBUTE((visibility("default")))
void ffrt_wake_coroutine(void *taskin)
{
#ifdef FFRT_BBOX_ENABLE
    TaskWakeCounterInc();
#endif
    ffrt::ffrt_executor_io_task *task = static_cast<ffrt::ffrt_executor_io_task*>(taskin);
    ffrt::ExecTaskStatus executing_status = ffrt::ExecTaskStatus::ET_EXECUTING;
    ffrt::ExecTaskStatus pending_status = ffrt::ExecTaskStatus::ET_PENDING;
    FFRT_LOGD("ffrt wake loop %d", task->status);
    std::lock_guard lg(task->lock);
    if (__atomic_compare_exchange_n(&(task->status), &executing_status, ffrt::ExecTaskStatus::ET_TOREADY, 0,
            __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
        return;
    }
    if (__atomic_compare_exchange_n(&(task->status), &pending_status, ffrt::ExecTaskStatus::ET_READY, 0,
            __ATOMIC_SEQ_CST, __ATOMIC_RELAXED)) {
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
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
            FFRT_LOGE("Submit io task failed!");
        }
#else
        ffrt::LinkedList* node = (ffrt::LinkedList *)(&task->wq);
        if (!ffrt::FFRTScheduler::Instance()->InsertNode(node, task->qos)) {
            FFRT_LOGE("Submit io task failed!");
        }
#endif
    }
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_coroutine_type(ffrt_task_attr_t* attr, ffrt_coroutine_t coroutine_type)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    return;
}

API_ATTRIBUTE((visibility("default")))
ffrt_coroutine_t ffrt_task_attr_get_coroutine_type(const ffrt_task_attr_t* attr)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return ffrt_coroutine_with_stack;
    }
    return ffrt_coroutine_stackless;
}
#ifdef __cplusplus
}
#endif

#endif