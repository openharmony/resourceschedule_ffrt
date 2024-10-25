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

/**
 * @brief worker num setting.
 *
 * @param qosData param is default when value equal 0xffffffff.
 * totalNum = lowQosReserveWorkerNum + highQosReserveWorkerNum + sum of all reserveNum
 * totalNum is valid in (0,256].
 * lowQosReserveWorkerNum is a low partition qos public resource.{[min, max], default} is {[0,256],12}.
 * highQosReserveWorkerNum is a hight partition qos public resource.{[min, max], default} is {[0,256],12}.
 * lowQosReserveWorkerNum is a global qos public resource.{[min, max], default} is {[0,256],24}.
 * qosConfigArray is an array of ffrt_qos_config.
 * effectLen: param setting will success when qosConfigArray index less than effectLen.
 * qos valid in [0,5].
 * reserveNum: mininum number which qos can create worker.{[min, max], default} is {[0,256],8}.
 * maxConcurrency is amx concurrency num of the qos.{[min, max], default} is {[0,12],8}.
 * hardLimit: max number which qos can create worker.{[min, max], default} is {[0,256],44}.
 * @return return 0 when setting success.return -1 when setting fail, and param is default.
 * @version 1.0
 */
FFRT_C_API int ffrt_set_qos_worker_num(ffrt_worker_num_param* qosData);

/**
 * @brief Set the task execution timeout.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param timeout_ms task execution timeout.
 * @version 1.0
 */
FFRT_C_API void ffrt_task_attr_set_timeout(ffrt_task_attr_t* attr, uint64_t timeout_ms);

/**
 * @brief Get the task execution timeout.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return Returns the task execution timeout.
 * @version 1.0
 */
FFRT_C_API uint64_t ffrt_task_attr_get_timeout(const ffrt_task_attr_t* attr);

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
