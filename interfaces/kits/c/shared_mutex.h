/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
 * @since 18
 */

/**
 * @file shared_mutex.h
 *
 * @brief Declares the shared mutex interfaces in C.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 18
 */

#ifndef FFRT_API_C_SHARED_MUTEX_H
#define FFRT_API_C_SHARED_MUTEX_H

#include "type_def.h"

/**
 * @brief Initializes a rwlock.
 *
 * The rwlock must later be destroyed by {@link ffrt_rwlock_destroy}.
 *
 * @param rwlock Indicates a pointer to the rwlock.
 * @param attr Indicates a pointer to the rwlock attribute.
 *             Currently, only the default mode is supported, set to null pointer.
 * @return `ffrt_success` if the rwlock is initialized and the attr is nullptr;
 *         `ffrt_error_inval` otherwise.
 * @since 18
 */
FFRT_C_API int ffrt_rwlock_init(ffrt_rwlock_t* rwlock, const ffrt_rwlockattr_t* attr);

/**
 * @brief Locks a write lock.
 *
 * Blocks the calling thread if the lock is unavailable. On success, the calling
 * thread holds the exclusive write lock until a matching call to {@link ffrt_rwlock_unlock}.
 * The write lock is exclusive: no read locks can be held concurrently.
 *
 * @param rwlock Indicates a pointer to the rwlock.
 * @return `ffrt_success` if the rwlock is locked;
 *         `ffrt_error_inval` if `rwlock` is a null pointer.
 * @see ffrt_rwlock_rdlock
 * @see ffrt_rwlock_trywrlock
 * @since 18
 */
FFRT_C_API int ffrt_rwlock_wrlock(ffrt_rwlock_t* rwlock);

/**
 * @brief Attempts to lock a write lock.
 *
 * Does not block the calling thread. On success, the calling thread holds the
 * exclusive write lock until a matching call to {@link ffrt_rwlock_unlock}.
 *
 * @param rwlock Indicates a pointer to the rwlock.
 * @return `ffrt_success` if the rwlock is locked;
 *         `ffrt_error_inval` or `ffrt_error_busy` otherwise.
 * @see ffrt_rwlock_wrlock
 * @since 18
 */
FFRT_C_API int ffrt_rwlock_trywrlock(ffrt_rwlock_t* rwlock);

/**
 * @brief Locks a read lock.
 *
 * Blocks the calling thread if the lock is unavailable. On success, the calling
 * thread holds a read lock until a matching call to {@link ffrt_rwlock_unlock}.
 * Multiple readers may hold the lock concurrently, but no writer may hold it.
 *
 * @param rwlock Indicates a pointer to the rwlock.
 * @return `ffrt_success` if the rwlock is locked;
 *         `ffrt_error_inval` if `rwlock` is a null pointer.
 * @see ffrt_rwlock_wrlock
 * @see ffrt_rwlock_tryrdlock
 * @since 18
 */
FFRT_C_API int ffrt_rwlock_rdlock(ffrt_rwlock_t* rwlock);

/**
 * @brief Attempts to lock a read lock.
 *
 * Does not block the calling thread. On success, the calling thread holds a
 * read lock until a matching call to {@link ffrt_rwlock_unlock}.
 *
 * @param rwlock Indicates a pointer to the rwlock.
 * @return `ffrt_success` if the rwlock is locked;
 *         `ffrt_error_inval` or `ffrt_error_busy` otherwise.
 * @see ffrt_rwlock_rdlock
 * @since 18
 */
FFRT_C_API int ffrt_rwlock_tryrdlock(ffrt_rwlock_t* rwlock);

/**
 * @brief Unlocks a rwlock.
 *
 * The rwlock must be held by the calling thread, having been previously locked by
 * {@link ffrt_rwlock_rdlock}, {@link ffrt_rwlock_tryrdlock}, {@link ffrt_rwlock_wrlock},
 * or {@link ffrt_rwlock_trywrlock}.
 *
 * @param rwlock Indicates a pointer to the rwlock.
 * @return `ffrt_success` if the rwlock is unlocked;
 *         `ffrt_error_inval` otherwise.
 * @since 18
 */
FFRT_C_API int ffrt_rwlock_unlock(ffrt_rwlock_t* rwlock);

/**
 * @brief Destroys a rwlock.
 *
 * The rwlock must have been initialized by {@link ffrt_rwlock_init} and no thread
 * may hold a read or write lock on entry.
 *
 * @param rwlock Indicates a pointer to the rwlock.
 * @return `ffrt_success` if the rwlock is destroyed;
 *         `ffrt_error_inval` otherwise.
 * @since 18
 */
FFRT_C_API int ffrt_rwlock_destroy(ffrt_rwlock_t* rwlock);

#endif // FFRT_API_C_SHARED_MUTEX_H
/** @} */
