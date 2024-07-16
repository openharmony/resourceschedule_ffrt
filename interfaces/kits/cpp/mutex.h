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
 * @file mutex.h
 *
 * @brief Declares the mutex interfaces in C++.
 *
 * @since 10
 * @version 1.0
 */
#ifndef FFRT_API_CPP_MUTEX_H
#define FFRT_API_CPP_MUTEX_H
#include "c/mutex.h"

namespace ffrt {
class mutex : public ffrt_mutex_t {
public:
    mutex()
    {
        ffrt_mutex_init(this, nullptr);
    }

    ~mutex()
    {
        ffrt_mutex_destroy(this);
    }

    mutex(mutex const&) = delete;
    void operator=(mutex const&) = delete;

    inline bool try_lock()
    {
        return ffrt_mutex_trylock(this) == ffrt_success ? true : false;
    }

    inline void lock()
    {
        ffrt_mutex_lock(this);
    }

    inline void unlock()
    {
        ffrt_mutex_unlock(this);
    }
};

class recursive_mutex : public ffrt_mutex_t {
public:
    recursive_mutex()
    {
        ffrt_recursive_mutex_init(this, nullptr);
    }

    ~recursive_mutex()
    {
        ffrt_recursive_mutex_destroy(this);
    }

    recursive_mutex(recursive_mutex const&) = delete;
    void operator=(recursive_mutex const&) = delete;

    inline bool try_lock()
    {
        return ffrt_recursive_mutex_trylock(this) == ffrt_success ? true : false;
    }

    inline void lock()
    {
        ffrt_recursive_mutex_lock(this);
    }

    inline void unlock()
    {
        ffrt_recursive_mutex_unlock(this);
    }
};
} // namespace ffrt
#endif
