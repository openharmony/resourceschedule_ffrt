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
 * @brief Provides FFRT C APIs.
 *
 * @since 12
 */

/**
 * @file loop.h
 *
 * @brief Declares the loop interfaces in C.
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
 * @brief Defines the loop handle, which identifies different loops.
 *
 * @since 12
 */
typedef void* ffrt_loop_t;

/**
 * @brief Creates a loop.
 *
 * @param queue Indicates a queue.
 * @return Returns a non-null loop handle if the loop is created;
           returns a null pointer otherwise.
 * @since 12
 */
FFRT_C_API ffrt_loop_t ffrt_loop_create(ffrt_queue_t queue);

/**
 * @brief Destroys a loop, the user needs to invoke this interface.
 *
 * @param loop Indicates a loop handle.
 * @return Returns 0 if the loop is destroyed;
           returns -1 otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_loop_destroy(ffrt_loop_t loop);

/**
 * @brief Starts a loop run.
 *
 * @param loop Indicates a loop handle.
 * @return Returns -1 if loop run fail;
           returns 0 otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_loop_run(ffrt_loop_t loop);

/**
 * @brief Stops a loop run.
 *
 * @param loop Indicates a loop handle.
 * @since 12
 */
FFRT_C_API void ffrt_loop_stop(ffrt_loop_t loop);

/**
 * @brief Controls an epoll file descriptor on ffrt loop.
 *
 * @warning Do not call `exit` in `cb` - this my cause unexpected behavior.
 *
 * @param loop Indicates a loop handle.
 * @param op Indicates operation on the target file descriptor.
 * @param fd Indicates the target file descriptor on which to perform the operation.
 * @param events Indicates the event type associated with the target file descriptor.
 * @param data Indicates user data used in cb.
 * @param cb Indicates user cb which will be executed when the target fd is polled.
 * @return Returns 0 if success;
           returns -1 otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_loop_epoll_ctl(ffrt_loop_t loop, int op, int fd, uint32_t events, void *data, ffrt_poller_cb cb);

/**
 * @brief Starts a timer on ffrt loop.
 *
 * @warning Do not call `exit` in `cb` - this my cause unexpected behavior.
 *
 * @param loop Indicates a loop handle.
 * @param timeout Indicates the number of milliseconds that specifies timeout.
 * @param data Indicates user data used in cb.
 * @param cb Indicates user cb which will be executed when timeout.
 * @param repeat Indicates whether to repeat this timer.
 * @return Returns a timer handle.
 * @since 12
 */
FFRT_C_API ffrt_timer_t ffrt_loop_timer_start(
    ffrt_loop_t loop, uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat);

/**
 * @brief Stops a timer on ffrt loop.
 *
 * @param loop Indicates a loop handle.
 * @param handle Indicates the target timer handle.
 * @return Returns 0 if success;
           returns -1 otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_loop_timer_stop(ffrt_loop_t loop, ffrt_timer_t handle);

#endif // FFRT_API_C_LOOP_H
/** @} */