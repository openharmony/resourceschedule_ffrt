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
#ifndef FFRT_API_CPP_DEADLINE_H
#define FFRT_API_CPP_DEADLINE_H
#include "c/deadline.h"

namespace ffrt {
using qos_interval_t = ffrt_qos_interval_t;

/**
    @brief app create an anonymous interval, the number is limited. should specify the deadline
*/
static inline qos_interval_t qos_interval_create(uint64_t deadline_us, int8_t qos = qos_default)
{
    return ffrt_qos_interval_create(deadline_us, static_cast<ffrt_qos_t>(qos));
}

/**
    @brief destroy a interval
*/
static inline void qos_interval_destroy(qos_interval_t it)
{
    ffrt_qos_interval_destroy(it);
}

/**
    @brief start the interval
*/
static inline int qos_interval_begin(qos_interval_t it)
{
    return ffrt_qos_interval_begin(it);
}

/**
    @brief update interval
*/
static inline void qos_interval_update(qos_interval_t it, uint64_t new_deadline_us)
{
    ffrt_qos_interval_update(it, new_deadline_us);
}

/**
    @brief interval become inactive util next begin
*/
static inline void qos_interval_end(qos_interval_t it)
{
    ffrt_qos_interval_end(it);
}

/**
    @brief current task or thread join an interval, only allow FIXED number of threads to join a interval
*/
static inline void qos_interval_join(qos_interval_t it)
{
    ffrt_qos_interval_join(it);
}

/**
    @brief current task or thread leave an interval
*/
static inline void qos_interval_leave(qos_interval_t it)
{
    ffrt_qos_interval_leave(it);
}

}; // namespace ffrt

#endif
