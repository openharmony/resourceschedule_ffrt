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
 * @addtogroup FFRT
 * @{
 *
 * @brief Provides Function Flow Runtime (FFRT) C APIs.
 *
 * FFRT is a task-based concurrent runtime library that automatically schedules
 * tasks according to their dependencies, eliminating the need for manual
 * thread management.
 *
 * @since 12
 */

/**
 * @file timer.h
 *
 * @brief Declares the timer interfaces in C.
 *
 * Provides timer capabilities based on QoS levels, supporting callback execution after a specified timeout.
 * It can be used for delayed task scheduling and other scenarios.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 12
 */

#ifndef FFRT_API_C_TIMER_H
#define FFRT_API_C_TIMER_H

#include <stdbool.h>
#include "type_def.h"

/**
 * @brief Starts a timer on an FFRT worker.
 *
 * Avoid calling `exit` or {@link ffrt_timer_stop} in `cb` to prevent undefined behavior or deadlock.
 *
 * @param qos Indicates the QoS of the worker that runs timer.
 * @param timeout Indicates the number of milliseconds that specifies timeout.
 * @param data Indicates user data used in cb.
 * @param cb Indicates user cb which will be executed when timeout.
 * @param repeat Indicates whether to repeat this timer. `true` to repeat the timer, `false` to run it once.
 * @return A timer handle; `-1` if the callback function is a null pointer or the QoS mapping is not registered.
 * @see ffrt_timer_stop
 * @since 12
 */
FFRT_C_API ffrt_timer_t ffrt_timer_start(ffrt_qos_t qos, uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat);

/**
 * @brief Stops a timer on an FFRT worker.
 *
 * This is a blocking interface. Avoid calling it inside the callback function to prevent deadlock
 * or synchronization issues. If the callback associated with `handle` is currently running,
 * this function waits for the callback to complete before returning.
 *
 * @param qos Indicates the QoS of the worker that runs the timer. Must match the QoS used in {@link ffrt_timer_start}.
 * @param handle Indicates the target timer handle returned by {@link ffrt_timer_start}.
 * @return `0` if success;
 *         `-1` otherwise.
 * @see ffrt_timer_start
 * @since 12
 */
FFRT_C_API int ffrt_timer_stop(ffrt_qos_t qos, ffrt_timer_t handle);

#endif // FFRT_API_C_TIMER_H
/** @} */
