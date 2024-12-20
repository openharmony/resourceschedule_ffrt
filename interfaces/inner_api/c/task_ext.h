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

#ifndef FFRT_INNER_API_C_TASK_H
#define FFRT_INNER_API_C_TASK_H
#include <stdint.h>
#include <stdbool.h>
#include "type_def_ext.h"

/**
 * @brief Skips a task.
 *
 * @param handle Indicates a task handle.
 * @return Returns <b>0</b> if the task is skipped;
           returns <b>-1</b> otherwise.
 * @since 10
 * @version 1.0
 */
FFRT_C_API int ffrt_skip(ffrt_task_handle_t handle);

// config
FFRT_C_API int ffrt_set_cgroup_attr(ffrt_qos_t qos, ffrt_os_sched_attr* attr);
FFRT_C_API void ffrt_restore_qos_config(void);
FFRT_C_API int ffrt_set_cpu_worker_max_num(ffrt_qos_t qos, uint32_t num);

/**
 * @brief Sets whether the task notifies worker, only support for normal task.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param notify Indicates whether the task notifies worker.
 * @version 1.0
 */
FFRT_C_API void ffrt_task_attr_set_notify_worker(ffrt_task_attr_t* attr, bool notify);

/**
 * @brief Notifies a specified number of workers at a specified QoS level.
 *
 * @param qos Indicates the QoS.
 * @param number Indicates the number of workers to be notified.
 * @version 1.0
 */
FFRT_C_API void ffrt_notify_workers(ffrt_qos_t qos, int number);

/**
 * @brief Obtains the ID of this queue.
 *
 * @return Returns the queue ID.
 * @version 1.0
 */
FFRT_C_API int64_t ffrt_this_queue_get_id(void);
#endif
