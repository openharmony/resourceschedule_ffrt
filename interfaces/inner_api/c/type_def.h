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
 
 /**
 * @file type_def.h
 *
 * @brief Declares common types.
 *
 * @since 10
 * @version 1.0
 */
#ifndef FFRT_INNER_API_C_TYPE_DEF_H
#define FFRT_INNER_API_C_TYPE_DEF_H
#include <stdint.h>
#include <errno.h>
#include "../../kits/c/type_def.h"

#ifdef __cplusplus
#define FFRT_C_API  extern "C"
#else
#define FFRT_C_API
#endif

/**
 * @brief Enumerates the task QoS types.
 *
 */
typedef enum {
    ffrt_qos_deadline_request = 4,
    ffrt_qos_user_interactive,
    ffrt_inner_qos_max = ffrt_qos_user_interactive,
} ffrt_inner_qos_default_t;

typedef enum {
    ffrt_stack_protect_weak,
    ffrt_stack_protect_strong
} ffrt_stack_protect_t;

typedef enum {
    ffrt_thread_attr_storage_size = 64,
} ffrt_inner_storage_size_t;

typedef struct {
    uint32_t storage[(ffrt_thread_attr_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_thread_attr_t;

#define MAX_CPUMAP_LENGTH 100 // this is in c and code style
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
enum qos_inner_default {
    qos_deadline_request = ffrt_qos_deadline_request,
    qos_user_interactive = ffrt_qos_user_interactive,
    qos_max = ffrt_inner_qos_max,
};

enum class stack_protect {
    weak = ffrt_stack_protect_weak,
    strong = ffrt_stack_protect_strong,
};
}
#endif
#endif
