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
#ifndef FFRT_API_C_TYPE_DEF_H
#define FFRT_API_C_TYPE_DEF_H
#include <stdint.h>
#include <errno.h>

#ifdef __cplusplus
#define FFRT_C_API  extern "C"
#else
#define FFRT_C_API
#endif

typedef enum{
    ffrt_coroutine_stackless,
    ffrt_coroutine_stackfull,
}ffrt_coroutine_t;

typedef enum{
    ffrt_ready=1,
    ffrt_blocked=3,
    ffrt_exited=4,
}ffrt_task_status_t;

typedef enum{
    ffrt_coroutine_pending=0,
    ffrt_coroutine_ready=1,
}ffrt_coroutine_ret_t;

typedef enum {
    ffrt_qos_inherit = -1,
    ffrt_qos_background,
    ffrt_qos_utility,
    ffrt_qos_default,
    ffrt_qos_user_initiated,
    ffrt_qos_deadline_request,
    ffrt_qos_user_interactive,
    ffrt_qos_defined_ive,
} ffrt_qos_t;

typedef enum {
    ffrt_stack_protect_weak,
    ffrt_stack_protect_strong
} ffrt_stack_protect_t;

typedef void(*ffrt_function_t)(void*);
typedef void(*ffrt_function_ptr_t)(void*);
typedef ffrt_coroutine_ret_t(*ffrt_coroutine_ptr_t)(void*);
typedef struct {
    ffrt_function_t exec;
    ffrt_function_t destroy;
    uint64_t reserve[2];
} ffrt_function_header_t;

typedef enum {
    ffrt_task_attr_storage_size = 128,
    ffrt_auto_managed_function_storage_size = 64 + sizeof(ffrt_function_header_t),
    ffrt_mutex_storage_size = 64,
    ffrt_cond_storage_size = 64,
    ffrt_thread_attr_storage_size = 64,
    ffrt_queue_attr_storage_size = 128,
} ffrt_storage_size_t;

typedef enum {
    ffrt_function_kind_general,
    ffrt_function_kind_queue
} ffrt_function_kind_t;

typedef enum {
    ffrt_dependence_data,
    ffrt_dependence_task,
} ffrt_dependence_type_t;

typedef struct {
    ffrt_dependence_type_t type;
    const void* ptr;
} ffrt_dependence_t;

typedef struct {
    uint32_t len;
    const ffrt_dependence_t* items;
} ffrt_deps_t;

typedef struct {
    uint32_t storage[(ffrt_task_attr_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_task_attr_t;

typedef struct {
    uint32_t storage[(ffrt_queue_attr_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_queue_attr_t;

typedef void* ffrt_task_handle_t;

typedef enum {
    ffrt_error = -1,
    ffrt_success = 0,
    ffrt_error_nomem = ENOMEM,
    ffrt_error_timedout = ETIMEDOUT,
    ffrt_error_busy = EBUSY,
    ffrt_error_inval = EINVAL
} ffrt_error_t;

typedef struct {
    long storage;
} ffrt_condattr_t;

typedef struct {
    long storage;
} ffrt_mutexattr_t;

typedef struct {
    uint32_t storage[(ffrt_thread_attr_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_thread_attr_t;

typedef struct {
    uint32_t storage[(ffrt_mutex_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_mutex_t;

typedef struct {
    uint32_t storage[(ffrt_cond_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_cond_t;

constexpr unsigned int MAX_CPUMAP_LENGTH = 100; // this is in c and code style
typedef struct {
    int shares;
    int latency_nice;
    int uclamp_min;
    int uclamp_max;
    int vip_prio;
    char cpumap[MAX_CPUMAP_LENGTH];
} ffrt_os_sched_attr;

typedef void* ffrt_thread_t;

typedef void* ffrt_interval_t;

typedef enum {
    ffrt_sys_event_type_read,
} ffrt_sys_event_type_t;

typedef enum {
    ffrt_sys_event_status_no_timeout,
    ffrt_sys_event_status_timeout
} ffrt_sys_event_status_t;

typedef void* ffrt_sys_event_handle_t;

typedef void* ffrt_config_t;

#ifdef __cplusplus
namespace ffrt {
enum qos {
    qos_inherit = ffrt_qos_inherit,
    qos_background = ffrt_qos_background,
    qos_utility = ffrt_qos_utility,
    qos_default = ffrt_qos_default,
    qos_user_initiated = ffrt_qos_user_initiated,
    qos_deadline_request = ffrt_qos_deadline_request,
    qos_user_interactive = ffrt_qos_user_interactive,
    qos_defined_ive = ffrt_qos_defined_ive,
};

enum class stack_protect {
    weak = ffrt_stack_protect_weak,
    strong = ffrt_stack_protect_strong,
};
}
#endif
#endif
