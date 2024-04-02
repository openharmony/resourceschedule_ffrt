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

static inline int set_cpu_worker_max_num(qos qos_, uint32_t num)
{
    return ffrt_set_cpu_worker_max_num(qos_, num);
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
