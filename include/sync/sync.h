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

#ifndef UTIL_SYNC_HPP
#define UTIL_SYNC_HPP
// Provide synchronization primitives

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include "sched/execute_ctx.h"
#include "cpp/fast_mutex.h"

namespace ffrt {
class spin_mutex {
    std::atomic<int> l;
    void lock_contended();

public:
    spin_mutex() : l(sync_detail::UNLOCK)
    {
    }
    spin_mutex(spin_mutex const&) = delete;
    void operator=(spin_mutex const&) = delete;

    void lock()
    {
        if (l.exchange(sync_detail::LOCK, std::memory_order_acquire) == sync_detail::UNLOCK) {
            return;
        }
        lock_contended();
    }

    void unlock()
    {
        l.store(sync_detail::UNLOCK, std::memory_order_release);
    }
};

bool DelayedWakeup(const TimePoint& to, WaitEntry* we, const std::function<void(WaitEntry*)>& wakeup,
    bool skipTimeCheck = false);
bool DelayedRemove(const TimePoint& to, WaitEntry* we);
} // namespace ffrt
#endif
