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
 * @file task.h
 *
 * @brief Declares the task inner interfaces in C++.
 *
 * @since 10
 * @version 1.0
 */
#ifndef FFRT_INNER_API_CPP_TASK_H
#define FFRT_INNER_API_CPP_TASK_H
#include <cstdint>
#include "c/task_ext.h"
#include "cpp/task.h"

namespace ffrt {
/**
 * @brief Skips a task.
 *
 * @param handle Indicates a task handle.
 * @return Returns <b>0</b> if the task is skipped;
           returns <b>-1</b> otherwise.
 * @since 10
 * @version 1.0
 */
static inline int skip(task_handle &handle)
{
    return ffrt_skip(handle);
}

void sync_io(int fd);

void set_trace_tag(const char* name);

void clear_trace_tag();

static inline int set_cgroup_attr(qos qos_, ffrt_os_sched_attr *attr)
{
    return ffrt_set_cgroup_attr(qos_, attr);
}

static inline void restore_qos_config()
{
    ffrt_restore_qos_config();
}

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
 * @return return ture when setting success.return false when setting fail, and param is default.
 * @version 1.0
 */
static inline int set_qos_worker_num(ffrt_worker_num_param* qosData)
{
    return ffrt_set_qos_worker_num(qosData);
}

/**
 * @brief Notifies a specified number of workers at a specified QoS level.
 *
 * @param qos_ Indicates the QoS.
 * @param number Indicates the number of workers to be notified.
 * @version 1.0
 */
static inline void notify_workers(qos qos_, int number)
{
    return ffrt_notify_workers(qos_, number);
}

/**
 * @brief Obtains the ID of this queue.
 *
 * @return Returns the queue ID.
 * @version 1.0
 */
static inline int64_t get_queue_id()
{
    return ffrt_this_queue_get_id();
}
} // namespace ffrt
#endif
