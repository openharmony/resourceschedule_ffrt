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
    ffrt_uv_task, // only used to register func for libuv
    ffrt_serial_task
} ffrt_executor_task_type_t;

typedef struct ffrt_executor_task {
    uintptr_t reserved[2];
    uintptr_t type; // 0: TaskCtx, 1: io task, User Space Address: libuv work
    void* wq[2];
} ffrt_executor_task_t;

typedef void (*ffrt_executor_task_func)(ffrt_executor_task_t* data, ffrt_qos_t qos);

FFRT_C_API void ffrt_executor_task_register_func(ffrt_executor_task_func func, ffrt_executor_task_type_t type);
FFRT_C_API void ffrt_executor_task_submit(ffrt_executor_task_t *task, const ffrt_task_attr_t *attr);
FFRT_C_API int ffrt_executor_task_cancel(ffrt_executor_task_t *task, const ffrt_qos_t qos);

#ifdef FFRT_IO_TASK_SCHEDULER
// poller
typedef void (*ffrt_poller_cb)(void*, uint32_t, uint8_t);
typedef void (*ffrt_timer_cb)(void*);
FFRT_C_API int ffrt_poller_register(int fd, uint32_t events, void* data, ffrt_poller_cb cb);
FFRT_C_API int ffrt_poller_deregister(int fd);
FFRT_C_API int ffrt_timer_start(uint64_t timeout, void* data, ffrt_timer_cb cb);
FFRT_C_API void ffrt_timer_stop(int handle);
FFRT_C_API ffrt_timer_query_t ffrt_timer_query(int handle);
FFRT_C_API void ffrt_poller_wakeup();

FFRT_C_API void ffrt_submit_coroutine(void* co, ffrt_coroutine_ptr_t exec,
    ffrt_function_t destroy, const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr);

// waker
FFRT_C_API void ffrt_wake_coroutine(void* task);

// get
FFRT_C_API void* ffrt_get_current_task();

#endif
#endif