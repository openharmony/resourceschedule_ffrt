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
 * @file mutex.h
 *
 * @brief Declares the mutex interfaces in C, which provide mutual exclusion between concurrent
 *        tasks to protect shared resources from race conditions.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 10
 */

#ifndef FFRT_API_C_MUTEX_H
#define FFRT_API_C_MUTEX_H

#include "type_def.h"

/**
 * @brief Initializes a mutex attribute.
 *
 * After successful initialization, the mutex attribute is set to its default value.
 * The mutex attribute must later be destroyed by {@link ffrt_mutexattr_destroy}.
 *
 * @param attr Indicates a pointer to the mutex attribute.
 * @return `ffrt_success` if the mutex attribute is initialized;
 *         `ffrt_error_inval` otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_mutexattr_init(ffrt_mutexattr_t* attr);

/**
 * @brief Sets the type of a mutex attribute.
 *
 * The type can be `ffrt_mutex_normal` (a regular mutex) or `ffrt_mutex_recursive`
 * (a recursive mutex that allows the same task to acquire the lock multiple times).
 *
 * @param attr Indicates a pointer to the mutex attribute.
 * @param type Indicates the mutex type, which can be `ffrt_mutex_normal`, `ffrt_mutex_recursive`,
 *             or `ffrt_mutex_default` (equivalent to `ffrt_mutex_normal`).
 * @return `ffrt_success` if the mutex attribute type is set successfully;
 *         `ffrt_error_inval` if attr is a null pointer or
 *         the mutex attribute type is not `ffrt_mutex_normal` or `ffrt_mutex_recursive`.
 * @see ffrt_mutex_type
 * @since 12
 */
FFRT_C_API int ffrt_mutexattr_settype(ffrt_mutexattr_t* attr, int type);

/**
 * @brief Gets the type of a mutex attribute.
 *
 * After a successful call, the type value is written to the out parameter `type`.
 *
 * @param attr Indicates a pointer to the mutex attribute.
 * @param type Indicates a pointer to the mutex type, used to receive the retrieved
 *        type value (`ffrt_mutex_normal` or `ffrt_mutex_recursive`).
 * @return `ffrt_success` if the mutex attribute type is retrieved successfully;
 *         `ffrt_error_inval` if attr or type is a null pointer.
 * @since 12
 */
FFRT_C_API int ffrt_mutexattr_gettype(ffrt_mutexattr_t* attr, int* type);

/**
 * @brief Destroys a mutex attribute.
 *
 * The mutex attribute must have been initialized by {@link ffrt_mutexattr_init}.
 *
 * @param attr Indicates a pointer to the mutex attribute.
 * @return `ffrt_success` if the mutex attribute is destroyed;
 *         `ffrt_error_inval` otherwise.
 * @since 12
 */
FFRT_C_API int ffrt_mutexattr_destroy(ffrt_mutexattr_t* attr);

/**
 * @brief Initializes a mutex.
 *
 * The mutex must later be destroyed by {@link ffrt_mutex_destroy}. Use `attr` to
 * pass a configured mutex attribute, or a null pointer to use defaults.
 *
 * @param mutex Indicates a pointer to the mutex.
 * @param attr Indicates a pointer to the mutex attribute, or a null pointer to use defaults.
 * @return `ffrt_success` if the mutex is initialized;
 *         `ffrt_error_inval` if `mutex` is null, or `attr` is non-null but does not specify
 *         a valid mutex type.
 * @since 10
 */
FFRT_C_API int ffrt_mutex_init(ffrt_mutex_t* mutex, const ffrt_mutexattr_t* attr);

/**
 * @brief Locks a mutex.
 *
 * If the mutex is already held by another thread, blocks the calling
 * thread until the mutex becomes available. On success, the calling thread
 * holds the mutex until a matching call to {@link ffrt_mutex_unlock}.
 *
 * @param mutex Indicates a pointer to the mutex.
 * @return `ffrt_success` if the mutex is locked;
 *         `ffrt_error_inval` otherwise.
 * @see ffrt_mutex_trylock
 * @since 10
 */
FFRT_C_API int ffrt_mutex_lock(ffrt_mutex_t* mutex);

/**
 * @brief Unlocks a mutex.
 *
 * The mutex must be held by the calling thread, having been previously locked by
 * {@link ffrt_mutex_lock} or {@link ffrt_mutex_trylock}.
 *
 * @param mutex Indicates a pointer to the mutex.
 * @return `ffrt_success` if the mutex is unlocked;
 *         `ffrt_error_inval` otherwise.
 * @since 10
 */
FFRT_C_API int ffrt_mutex_unlock(ffrt_mutex_t* mutex);

/**
 * @brief Attempts to lock a mutex.
 *
 * This is a non-blocking operation: if the mutex is held
 * by another thread, the function returns immediately with an error code.
 * On success, the calling thread holds the mutex until a matching call to
 * {@link ffrt_mutex_unlock}.
 *
 * @param mutex Indicates a pointer to the mutex.
 * @return `ffrt_success` if the mutex is locked;
 *         `ffrt_error_inval` or `ffrt_error_busy` otherwise.
 * @see ffrt_mutex_lock
 * @since 10
 */
FFRT_C_API int ffrt_mutex_trylock(ffrt_mutex_t* mutex);

/**
 * @brief Destroys a mutex.
 *
 * After a successful call, the resources occupied by the mutex are
 * released and the mutex object can no longer be used. The mutex must have
 * been initialized by {@link ffrt_mutex_init} and no thread may hold it on entry.
 *
 * @param mutex Indicates a pointer to the mutex.
 * @return `ffrt_success` if the mutex is destroyed;
 *         `ffrt_error_inval` otherwise.
 * @since 10
 */
FFRT_C_API int ffrt_mutex_destroy(ffrt_mutex_t* mutex);

#endif // FFRT_API_C_MUTEX_H
/** @} */
