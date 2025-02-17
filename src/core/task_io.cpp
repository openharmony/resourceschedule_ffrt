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
#include "tm/io_task.h"
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

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
void ffrt_submit_coroutine(void* co, ffrt_coroutine_ptr_t exec, ffrt_function_t destroy, const ffrt_deps_t* in_deps,
    const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr)
{
    FFRT_COND_DO_ERR((exec == nullptr), return, "input invalid, exec == nullptr");
    FFRT_COND_DO_ERR((destroy == nullptr), return, "input invalid, destroy == nullptr");

    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));

    (void)in_deps;
    (void)out_deps;
    ffrt::ffrt_io_callable_t work;
    work.exec = exec;
    work.destroy = destroy;
    work.data = co;
    if (likely(attr == nullptr || ffrt_task_attr_get_delay(attr) == 0)) {
        ffrt::FFRTFacade::GetDMInstance().onSubmitIO(work, p);
        return;
    }
    FFRT_LOGE("io function does not support delay");
}

API_ATTRIBUTE((visibility("default")))
void* ffrt_get_current_task(void)
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

    ffrt::IOTask* wakedTask = static_cast<ffrt::IOTask*>(task);
    wakedTask->status = ffrt::ExecTaskStatus::ET_READY;

#ifdef FFRT_LOCAL_QUEUE_ENABLE
    // in self-wakeup scenario, tasks are placed in local fifo to delay scheduling, implementing the yeild function
    bool selfWakeup = (ffrt::ExecuteCtx::Cur()->exec_task == task);
    if (!selfWakeup) {
        if (ffrt::ExecuteCtx::Cur()->PushTaskToPriorityStack(wakedTask)) {
            return;
        }

        if (rand() % INSERT_GLOBAL_QUEUE_FREQ) {
            if (ffrt::ExecuteCtx::Cur()->localFifo != nullptr &&
                ffrt::ExecuteCtx::Cur()->localFifo->PushTail(task) == 0) {
                ffrt::FFRTFacade::GetEUInstance().NotifyLocalTaskAdded(wakedTask->qos_);
                return;
            }
        }
    }
#endif

    if (!ffrt::FFRTFacade::GetSchedInstance()->InsertNode(&wakedTask->fq_we.node, wakedTask->qos_)) {
        FFRT_LOGE("Submit io task failed!");
    }
}
#ifdef __cplusplus
}
#endif
