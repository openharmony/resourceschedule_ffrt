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
#ifndef FFRT_API_C_EXECUTOR_TASK_H
#define FFRT_API_C_EXECUTOR_TASK_H

#include "type_def.h"

typedef enum {
    ffrt_normal_task = 0,
    ffrt_rust_task = 1,
    ffrt_uv_task // only used to register func for libuv
} ffrt_executor_task_type_t;

typedef struct ffrt_executor_task {
    uintptr_t reserved[2];
    uintptr_t type; // 0: TaskCtx, 1: rust task, User Space Address: libuv work
    void* wq[2];
} ffrt_executor_task_t;

typedef void (*ffrt_executor_task_func)(ffrt_executor_task_t* data);

FFRT_C_API void ffrt_executor_task_register_func(ffrt_executor_task_func func, const char* name);
FFRT_C_API void ffrt_executor_task_submit(ffrt_executor_task_t *task, const ffrt_task_attr_t *attr);
FFRT_C_API int ffrt_executor_task_cancel(ffrt_executor_task_t *task, const ffrt_qos_t qos);

#endif