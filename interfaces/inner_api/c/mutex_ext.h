/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef FFRT_INNER_API_C_MUTEX_EXT_H
#define FFRT_INNER_API_C_MUTEX_EXT_H

/**
 * @brief Locks a mutex throuth the slow path.
 *
 * @param mutex Indicates a pointer to the mutex.
 * @return Returns <b>ffrt_success</b> if the mutex is locked;
           returns <b>ffrt_error_inval</b> or blocks the calling thread otherwise.
 * @since 23
 */
FFRT_C_API int ffrt_mutex_lock_wait(ffrt_mutex_t* mutex);

/**
 * @brief Unlocks a mutex.
 *
 * @param mutex Indicates a pointer to the mutex.
 * @return Returns <b>ffrt_success</b> if the mutex is unlocked;
           returns <b>ffrt_error_inval</b> otherwise.
 * @since 23
 */
FFRT_C_API int ffrt_mutex_unlock_wake(ffrt_mutex_t* mutex);

#endif