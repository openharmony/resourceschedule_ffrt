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

#ifndef FFRT_TASK_IO_H
#define FFRT_TASK_IO_H

#include "internal_inc/types.h"
#include "sched/task_state.h"
#include "sched/interval.h"
#include "task_attr_private.h"
#include "util/slab.h"
#include "c/executor_task.h"

#ifdef FFRT_IO_TASK_SCHEDULER
namespace ffrt {
typedef struct {
    ffrt_function_t exec;
    ffrt_function_t destroy;
    void* callable;
} ffrt_io_callable_t;

struct ffrt_executor_io_task: public ffrt_executor_task {
    ffrt_executor_io_task(const QoS &qos) : qos(qos)
    {
        type = ffrt_io_task;
    }
    bool wakeFlag = true;
    bool withHandle = true;
    uint8_t func_storage[ffrt_auto_managed_function_storage_size]; // 函数闭包、指针或函数对象
    ffrt_io_callable_t wake_callable_on_finish {nullptr, nullptr, nullptr};
    // 本task finished时需要wake的task，用于无栈协程子任务完成时wake父任务
    QoS qos;
    ExecTaskStatus status = ExecTaskStatus::ET_PENDING;
    fast_mutex lock; // used in coroute
    inline void freeMem()
    {
        SimpleAllocator<ffrt_executor_io_task>::freeMem(this);
    }
    void SetWakeFlag(bool wakeFlagIn)
    {
        wakeFlag = wakeFlagIn;
    }
};
} /* namespace ffrt */
#endif
#endif