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
 * @file shared_mutex.h
 *
 * @brief Declares the shared_mutex interfaces in C++.
 *
 * @version 1.0
 */

#ifndef FFRT_API_CPP_SHARED_MUTEX_H
#define FFRT_API_CPP_SHARED_MUTEX_H
#include "c/shared_mutex.h"

namespace ffrt {
class shared_mutex : public ffrt_rwlock_t {
public:
    shared_mutex()
    {
        ffrt_rwlock_init(this, nullptr);
    }

    ~shared_mutex()
    {
        ffrt_rwlock_destroy(this);
    }

    shared_mutex(shared_mutex const&) = delete;
    void operator=(shared_mutex const&) = delete;

    inline void lock()
    {
        ffrt_rwlock_wrlock(this);
    }

    inline bool try_lock()
    {
        return ffrt_rwlock_trywrlock(this) == ffrt_success ? true : false;
    }

    inline void unlock()
    {
        ffrt_rwlock_unlock(this);
    }

    inline void lock_shared()
    {
        ffrt_rwlock_rdlock(this);
    }

    inline bool try_lock_shared()
    {
        return ffrt_rwlock_tryrdlock(this) == ffrt_success ? true : false;
    }

    inline void unlock_shared()
    {
        ffrt_rwlock_unlock(this);
    }
};
} // namespace ffrt
#endif
