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
#ifndef FFRT_API_C_QUEUE_H
#define FFRT_API_C_QUEUE_H

#include "type_def.h"

typedef enum { ffrt_queue_serial, ffrt_queue_max } ffrt_queue_type_t;
typedef void* ffrt_queue_t;

// attr
FFRT_C_API int ffrt_queue_attr_init(ffrt_queue_attr_t* attr);
FFRT_C_API void ffrt_queue_attr_destroy(ffrt_queue_attr_t* attr);
FFRT_C_API void ffrt_queue_attr_set_qos(ffrt_queue_attr_t* attr, ffrt_qos_t qos);
FFRT_C_API ffrt_qos_t ffrt_queue_attr_get_qos(const ffrt_queue_attr_t* attr);
FFRT_C_API void ffrt_queue_attr_set_timeout(ffrt_queue_attr_t* attr, uint64_t timeout_us);
FFRT_C_API uint64_t ffrt_queue_attr_get_timeout(const ffrt_queue_attr_t* attr);
FFRT_C_API void ffrt_queue_attr_set_timeoutCb(ffrt_queue_attr_t* attr, ffrt_function_header_t* f);
FFRT_C_API ffrt_function_header_t* ffrt_queue_attr_get_timeoutCb(const ffrt_queue_attr_t* attr);

// create serial queue
FFRT_C_API ffrt_queue_t ffrt_queue_create(ffrt_queue_type_t type, const char* name, const ffrt_queue_attr_t* attr);

// destroy serial queue
FFRT_C_API void ffrt_queue_destroy(ffrt_queue_t queue);

// submit to serial queue
FFRT_C_API void ffrt_queue_submit(ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr);
FFRT_C_API ffrt_task_handle_t ffrt_queue_submit_h(
    ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr);

// wait serial task
FFRT_C_API void ffrt_queue_wait(ffrt_task_handle_t handle);

// cancel serial task
FFRT_C_API int ffrt_queue_cancel(ffrt_task_handle_t handle);

#endif // FFRT_API_C_QUEUE_H