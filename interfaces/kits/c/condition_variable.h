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
 * @since 10
 */

/**
 * @file condition_variable.h
 *
 * @brief Declares the condition variable interfaces in C.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 10
 */

#ifndef FFRT_API_C_CONDITION_VARIABLE_H
#define FFRT_API_C_CONDITION_VARIABLE_H

#include <time.h>
#include "type_def.h"

/**
 * @brief Initializes a condition variable.
 *
 * The condition variable must later be destroyed by {@link ffrt_cond_destroy} when no longer in use.
 *
 * @param cond Indicates a pointer to the condition variable.
 * @param attr Indicates a pointer to the condition variable attribute.
 * @return `ffrt_success` if the condition variable is initialized;
 *         `ffrt_error_inval` otherwise.
 * @since 10
 */
FFRT_C_API int ffrt_cond_init(ffrt_cond_t* cond, const ffrt_condattr_t* attr);

/**
 * @brief Unblocks at least one of the threads that are blocked on a condition variable.
 *
 * @param cond Indicates a pointer to the condition variable.
 * @return `ffrt_success` if the thread is unblocked;
 *         `ffrt_error_inval` otherwise.
 * @see ffrt_cond_wait
 * @since 10
 */
FFRT_C_API int ffrt_cond_signal(ffrt_cond_t* cond);

/**
 * @brief Unblocks all threads currently blocked on a condition variable.
 *
 * @param cond Indicates a pointer to the condition variable.
 * @return `ffrt_success` if the threads are unblocked;
 *         `ffrt_error_inval` otherwise.
 * @see ffrt_cond_wait
 * @since 10
 */
FFRT_C_API int ffrt_cond_broadcast(ffrt_cond_t* cond);

/**
 * @brief Blocks the calling thread on a condition variable.
 *
 * The mutex must be held by the calling thread on entry. It is atomically released
 * while the thread is blocked, and re-acquired before the function returns, so the
 * caller regains ownership of the mutex on wakeup. The thread is unblocked by a
 * call to {@link ffrt_cond_signal} or {@link ffrt_cond_broadcast} from another thread.
 * The caller is responsible for re-checking the predicate after wakeup to guard
 * against spurious wakeups.
 *
 * @param cond Indicates a pointer to the condition variable.
 * @param mutex Indicates a pointer to the mutex held by the calling thread.
 * @return `ffrt_success` if the thread is unblocked after being blocked;
 *         `ffrt_error_inval` otherwise.
 * @see ffrt_cond_timedwait
 * @see ffrt_cond_signal
 * @see ffrt_cond_broadcast
 * @since 10
 */
FFRT_C_API int ffrt_cond_wait(ffrt_cond_t* cond, ffrt_mutex_t* mutex);

/**
 * @brief Blocks the calling thread until a given time point.
 *
 * If {@link ffrt_cond_signal} or {@link ffrt_cond_broadcast} is not called to unblock the thread
 * before `time_point` is reached, the thread is automatically unblocked.
 *
 * @param cond Indicates a pointer to the condition variable.
 * @param mutex Indicates a pointer to the mutex.
 * @param time_point Indicates the absolute time point at which the wait expires.
 * @return `ffrt_success` if the thread is unblocked after being blocked;
 *         `ffrt_error_timedout` if `time_point` is reached without being signaled;
 *         `ffrt_error_inval` if any of `cond`, `mutex`, or `time_point` is null.
 * @see ffrt_cond_wait
 * @see ffrt_cond_signal
 * @see ffrt_cond_broadcast
 * @since 10
 */
FFRT_C_API int ffrt_cond_timedwait(ffrt_cond_t* cond, ffrt_mutex_t* mutex, const struct timespec* time_point);

/**
 * @brief Destroys a condition variable.
 *
 * The condition variable must have been initialized by {@link ffrt_cond_init} and
 * must not be referenced by any thread on entry.
 *
 * @param cond Indicates a pointer to the condition variable.
 * @return `ffrt_success` if the condition variable is destroyed;
 *         `ffrt_error_inval` otherwise.
 * @since 10
 */
FFRT_C_API int ffrt_cond_destroy(ffrt_cond_t* cond);

#endif // FFRT_API_C_CONDITION_VARIABLE_H
/** @} */
