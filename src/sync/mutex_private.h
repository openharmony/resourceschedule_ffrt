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

#ifndef _MUTEX_PRIVATE_H_
#define _MUTEX_PRIVATE_H_

#include "sync/sync.h"

namespace ffrt {
class mutexPrivate {
    std::atomic<int> l;
    fast_mutex wlock;
    LinkedList list;

    void wait();
    void wake();

public:
    mutexPrivate() : l(sync_detail::UNLOCK) {}
    mutexPrivate(mutexPrivate const &) = delete;
    void operator = (mutexPrivate const &) = delete;

    bool try_lock();
    void lock();
    void unlock();
};
} // namespace ffrt

#endif