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

#include <stdint.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include "type_def_ext.h"
#include "c/timer.h"

typedef struct ffrt_executor_task {
    uintptr_t reserved[2];
    uintptr_t type; // 0: TaskCtx, 1~: Dynamicly Define Task, User Space Address: libuv work
    void* wq[2];
} ffrt_executor_task_t;

typedef enum {
    ffrt_normal_task = 0,
    ffrt_io_task = 1,
    ffrt_uv_task, // only used to register func for libuv
    ffrt_queue_task,
    ffrt_invalid_task
} ffrt_executor_task_type_t;

typedef void (*ffrt_executor_task_func)(ffrt_executor_task_t* data, ffrt_qos_t qos);
FFRT_C_API void ffrt_executor_task_register_func(ffrt_executor_task_func func, ffrt_executor_task_type_t type);
FFRT_C_API void ffrt_executor_task_submit(ffrt_executor_task_t* task, const ffrt_task_attr_t* attr);
FFRT_C_API int ffrt_executor_task_cancel(ffrt_executor_task_t* task, const ffrt_qos_t qos);

// poller
FFRT_C_API void ffrt_poller_wakeup(ffrt_qos_t qos);

FFRT_C_API uint8_t ffrt_epoll_get_count(ffrt_qos_t qos);

FFRT_C_API ffrt_timer_query_t ffrt_timer_query(ffrt_qos_t qos, ffrt_timer_t handle);

FFRT_C_API int ffrt_epoll_ctl(ffrt_qos_t qos, int op, int fd, uint32_t events, void* data, ffrt_poller_cb cb);

FFRT_C_API int ffrt_epoll_wait(ffrt_qos_t qos, struct epoll_event* events, int max_events, int timeout);

FFRT_C_API uint64_t ffrt_epoll_get_wait_time(void* taskHandle);

// ffrt_executor_task
FFRT_C_API void ffrt_submit_coroutine(void* co, ffrt_coroutine_ptr_t exec, ffrt_function_t destroy,
    const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr);

// waker
FFRT_C_API void ffrt_wake_coroutine(void* task);

// get
FFRT_C_API void* ffrt_get_current_task();

/**
 * @brief Obtains current coroutine stack address and size.
 *
 * @param stack_addr Coroutine stack address.
 * @param size Coroutine stack size.
 * @return Returns <b>0</b> if the stack is obtained;
 *         returns <b>-1</b> otherwise.
 * @since 12
 * @version 1.0
 */
FFRT_C_API bool ffrt_get_current_coroutine_stack(void** stack_addr, size_t* size);

/**
 * @brief Obtains current task.
 *
 * @param none.
 * @return Returns current task.
 * @since 12
 * @version 1.0
 */
FFRT_C_API void* ffrt_get_cur_task();

/**
 * @brief Set the taskLocal flag in ffrt_task_attr.
 *
 * @param attr The ffrt_task_attr struct.
 * @param task_local The bool value to be set.
 * @return none.
 * @since 12
 * @version 1.0
 */
FFRT_C_API void ffrt_task_attr_set_local(ffrt_task_attr_t* attr, bool task_local);

/**
 * @brief Obtains the taskLocal flag in ffrt_task_attr.
 *
 * @param attr The ffrt_task_attr struct.
 * @return The bool value of task_local.
 * @since 12
 * @version 1.0
 */
FFRT_C_API bool ffrt_task_attr_get_local(ffrt_task_attr_t* attr);

/**
 * @brief Obtains the thread id of the input task handle.
 *
 * @param task_handle The task pointer.
 * @return The thread id of the input task handle.
 * @version 1.0
 */
FFRT_C_API pthread_t ffrt_task_get_tid(void* task_handle);

/**
 * @brief Obtains the task id cached by the current thread.
 *
 * @return Returns the task id.
 * @version 1.0
 */
FFRT_C_API uint64_t ffrt_get_cur_cached_task_id();
#endif