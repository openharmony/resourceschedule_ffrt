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
    ffrt_io_task = 1,
    ffrt_uv_task // only used to register func for libuv
} ffrt_executor_task_type_t;

typedef struct ffrt_executor_task {
    uintptr_t reserved[2];
    uintptr_t type; // 0: TaskCtx, 1: io task, User Space Address: libuv work
    void* wq[2];
} ffrt_executor_task_t;

typedef void (*ffrt_executor_task_func)(ffrt_executor_task_t* data);

// ffrt_executor_task
FFRT_C_API void ffrt_executor_task_register_func(ffrt_executor_task_func func, ffrt_executor_task_type_t type);
FFRT_C_API void ffrt_executor_task_submit(ffrt_executor_task_t *task, const ffrt_task_attr_t *attr);
FFRT_C_API int ffrt_executor_task_cancel(ffrt_executor_task_t *taask, const ffrt_qos_t qos);

#ifdef FFRT_IO_TASK_SCHEDULER
// poller
FFRT_C_API int ffrt_poller_register(int fd, uint32_t events, void* data, void(*cb)(void*, uint32_t));
FFRT_C_API int ffrt_poller_deregister(int fd);
FFRT_C_API int ffrt_poller_register_timerfunc(int(*timerFunc)());
FFRT_C_API void ffrt_poller_wakeup();

FFRT_C_API void ffrt_submit_coroutine(void* co, ffrt_coroutine_ptr_t exec,
    ffrt_function_t destroy, const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr);
FFRT_C_API ffrt_task_handle_t ffrt_submit_h_coroutine(void* co, ffrt_coroutine_ptr_t exec,
    ffrt_function_t destroy, const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr);

// waker
FFRT_C_API void ffrt_wake_by_handle(void* waker, ffrt_function_t exec,
    ffrt_function_t destroy, ffrt_task_handle_t handle);
FFRT_C_API void ffrt_set_wake_flag(int flag);
FFRT_C_API void ffrt_wake_coroutine(void *task);

// get
FFRT_C_API void *ffrt_task_get(void);

#endif
#endif