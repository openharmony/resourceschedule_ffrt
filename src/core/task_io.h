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
#include "util/slab.h"
#include "c/executor_task.h"

#ifdef FFRT_IO_TASK_SCHEDULER
namespace ffrt {
typedef struct {
    ffrt_coroutine_ptr_t exec;
    ffrt_function_t destroy;
    void* data;
} ffrt_io_callable_t;

struct ffrt_executor_io_task: public ffrt_executor_task {
    ffrt_executor_io_task(const QoS &qos) : qos(qos)
    {
        type = ffrt_io_task;
        work = {nullptr, nullptr, nullptr};
    }

    QoS qos;
    ffrt_io_callable_t work;
    ExecTaskStatus status = ExecTaskStatus::ET_PENDING;
};
} /* namespace ffrt */
#endif
#endif