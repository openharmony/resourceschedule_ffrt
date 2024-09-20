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
#include "core/task_io.h"
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
#include <dlfcn.h>
#endif
#include "util/slab.h"
#include "util/ffrt_facade.h"
#include "util/spmc_queue.h"

#define ENABLE_LOCAL_QUEUE

namespace {
const int INSERT_GLOBAL_QUEUE_FREQ = 5;
}

namespace ffrt {
static void work_finish_callable(IOTaskExecutor* task)
{
    task->status = ExecTaskStatus::ET_FINISH;
    task->work.destroy(task->work.data);
    delete task;
}

static void ExecuteIOTask(ffrt_executor_task_t* data, ffrt_qos_t qos)
{
    IOTaskExecutor* task = static_cast<IOTaskExecutor*>(data);
    task->status = ExecTaskStatus::ET_EXECUTING;
    (void)qos;
    ffrt_coroutine_ptr_t coroutine = task->work.exec;
    ffrt_coroutine_ret_t ret = coroutine(task->work.data);
    if (ret == ffrt_coroutine_ready) {
        FFRT_EXECUTOR_TASK_FINISH_MARKER(task);
        work_finish_callable(task);
        return;
    }

    FFRT_EXECUTOR_TASK_BLOCK_MARKER(task);
    task->status = ffrt::ExecTaskStatus::ET_PENDING;
#ifdef FFRT_BBOX_ENABLE
    TaskPendingCounterInc();
#endif
}

static pthread_once_t once = PTHREAD_ONCE_INIT;

static void InitIOTaskExecutor()
{
    ffrt_executor_task_register_func(ExecuteIOTask, ffrt_io_task);
}
} /* namespace ffrt */

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
void ffrt_submit_coroutine(void* co, ffrt_coroutine_ptr_t exec, ffrt_function_t destroy, const ffrt_deps_t* in_deps,
    const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr)
{
    FFRT_COND_DO_ERR((exec == nullptr), return, "input invalid, exec == nullptr");
    pthread_once(&ffrt::once, ffrt::InitIOTaskExecutor);

    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    ffrt::QoS qos = (p == nullptr ? ffrt::QoS() : ffrt::QoS(p->qos_));

    (void)in_deps;
    (void)out_deps;
    ffrt::IOTaskExecutor* task = new ffrt::IOTaskExecutor(qos);
    task->work.exec = exec;
    task->work.destroy = destroy;
    task->work.data = co;
    task->status = ffrt::ExecTaskStatus::ET_READY;

    ffrt_executor_task_submit(dynamic_cast<ffrt_executor_task_t*>(task), attr);
}

API_ATTRIBUTE((visibility("default")))
void* ffrt_get_current_task()
{
    return reinterpret_cast<void*>(ffrt::ExecuteCtx::Cur()->exec_task);
}

// API used to schedule stackless coroutine task
API_ATTRIBUTE((visibility("default")))
void ffrt_wake_coroutine(void* task)
{
    if (task == nullptr) {
        FFRT_LOGE("Task is nullptr.");
        return;
    }

#ifdef FFRT_BBOX_ENABLE
    TaskWakeCounterInc();
#endif

    ffrt::IOTaskExecutor* wakedTask = static_cast<ffrt::IOTaskExecutor*>(task);
    wakedTask->status = ffrt::ExecTaskStatus::ET_READY;

    // in self-wakeup scenario, tasks are placed in local fifo to delay scheduling, implementing the yeild function
    bool selfWakeup = (ffrt::ExecuteCtx::Cur()->exec_task == task);
    if (!selfWakeup) {
        if (ffrt::ExecuteCtx::Cur()->PushTaskToPriorityStack(wakedTask)) {
            return;
        }

        if (rand() % INSERT_GLOBAL_QUEUE_FREQ) {
            if (ffrt::ExecuteCtx::Cur()->localFifo != nullptr &&
                ffrt::ExecuteCtx::Cur()->localFifo->PushTail(task) == 0) {
                ffrt::FFRTFacade::GetEUInstance().NotifyLocalTaskAdded(wakedTask->qos);
                return;
            }
        }
    }

    ffrt::LinkedList* node = reinterpret_cast<ffrt::LinkedList *>(&wakedTask->wq);
    if (!ffrt::FFRTFacade::GetSchedInstance()->InsertNode(node, wakedTask->qos)) {
        FFRT_LOGE("Submit io task failed!");
    }
}
#ifdef __cplusplus
}
#endif

