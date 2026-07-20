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
 * @file loop.h
 *
 * @brief Declares the event loop interfaces in C.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 12
 */

#ifndef FFRT_API_C_LOOP_H
#define FFRT_API_C_LOOP_H

#include <stdbool.h>
#include "type_def.h"
#include "queue.h"

/**
 * @brief Loop handle, which identifies different loops.
 *
 * @since 12
 */
typedef void* ffrt_loop_t;

/**
 * @brief Creates a loop on the specified queue for running an event loop.
 *
 * @param queue Indicates a queue.
 * @return A non-null loop handle if the loop is created;
 *         a null pointer otherwise.
 * @since 12
 */
FFRT_C_API ffrt_loop_t ffrt_loop_create(ffrt_queue_t queue);

/**
 * @brief Destroys a loop.
 *
 * Call this interface to release the resources associated with the loop.
 *
 * @param loop Indicates a loop handle.
 * @return `0` if the loop is destroyed;
 *         `-1` otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_loop_destroy(ffrt_loop_t loop);

/**
 * @brief Starts a loop run.
 *
 * This function occupies the calling thread, running the event loop synchronously on the
 * current thread until {@link ffrt_loop_stop} is invoked.
 *
 * @param loop Indicates a loop handle.
 * @return `0` if the loop run succeeds;
 *         `-1` otherwise.
 * @see ffrt_loop_stop
 * @since 12
 */
FFRT_C_API int ffrt_loop_run(ffrt_loop_t loop);

/**
 * @brief Stops a loop run.
 *
 * After this call, the thread executing {@link ffrt_loop_run} stops the loop and returns.
 *
 * @param loop Indicates a loop handle.
 * @see ffrt_loop_run
 * @since 12
 */
FFRT_C_API void ffrt_loop_stop(ffrt_loop_t loop);

/**
 * @brief Controls an epoll file descriptor on ffrt loop.
 *
 * Adds, modifies, or deletes the monitored events on the target file descriptor.
 *
 * @param loop Indicates a loop handle.
 * @param op Indicates the operation type on the target file descriptor, such as add, modify, or delete.
 * @param fd Indicates the target file descriptor on which to perform the operation.
 * @param events Indicates the event type to monitor on the target file descriptor
 *               (such as readable, writable, and so on), and can be combined by bitwise OR.
 * @param data Indicates user data used in cb.
 * @param cb Indicates user cb which will be executed when the target fd is polled.
 * @return `0` if the operation succeeds;
 *         `-1` otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_loop_epoll_ctl(ffrt_loop_t loop, int op, int fd, uint32_t events, void* data, ffrt_poller_cb cb);

/**
 * @brief Starts a timer on ffrt loop.
 *
 * The callback is invoked after the timeout elapses, and is repeated if `repeat` is `true`.
 *
 * @param loop Indicates a loop handle.
 * @param timeout Indicates the number of milliseconds that specifies timeout. The value range is [0, +∞).
 * @param data Indicates user data used in cb.
 * @param cb Indicates user cb which will be executed when timeout.
 * @param repeat Indicates whether to repeat this timer. `true` to repeat the timer, `false` to run it once.
 * @return The timer handle; `-1` if `loop` or `cb` is null.
 * @see ffrt_loop_timer_stop
 * @since 12
 */
FFRT_C_API ffrt_timer_t ffrt_loop_timer_start(
    ffrt_loop_t loop, uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat);

/**
 * @brief Stops a timer on ffrt loop.
 *
 * After this call, the timer no longer fires.
 *
 * @param loop Indicates a loop handle.
 * @param handle Indicates the timer handle returned by {@link ffrt_loop_timer_start}.
 * @return `0` if the operation succeeds;
 *         `-1` otherwise.
 * @see ffrt_loop_timer_start
 * @since 12
 */
FFRT_C_API int ffrt_loop_timer_stop(ffrt_loop_t loop, ffrt_timer_t handle);

#endif // FFRT_API_C_LOOP_H
/** @} */
