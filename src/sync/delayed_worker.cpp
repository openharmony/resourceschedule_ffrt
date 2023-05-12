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

#include "delayed_worker.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <thread>
#include <linux/futex.h>
namespace ffrt {

DelayedWorker::DelayedWorker() : futex(0)
{
    std::thread t([this]() {
        prctl(PR_SET_NAME, "delayed_worker");
        for (;;) {
            if (futex < 0) {
                break;
            }
            struct timespec ts;
            struct timespec *p = &ts;
            HandleWork(&p);
            syscall(SYS_futex, &futex, FUTEX_WAIT_BITSET, 0, p, 0, -1);
        }
    });
    t.detach();
}

DelayedWorker::~DelayedWorker()
{
    lock.lock();
    futex = -1;
    syscall(SYS_futex, &futex, FUTEX_WAKE, 1);
    lock.unlock();
}

void DelayedWorker::HandleWork(struct timespec** p)
{
    const int NS_PER_SEC = 1000000000;
    std::lock_guard<decltype(lock)> l(lock);

    while (!map.empty()) {
        time_point_t now = std::chrono::steady_clock::now();
        auto cur = map.begin();
        if (cur->first <= now) {
            DelayedWork w = cur->second;
            map.erase(cur);
            lock.unlock();
            (*w.cb)(w.we);
            lock.lock();
        } else {
            std::chrono::nanoseconds ns = cur->first.time_since_epoch();
            (*p)->tv_sec = ns.count() / NS_PER_SEC;
            (*p)->tv_nsec = ns.count() % NS_PER_SEC;
            futex = 0;
            return;
        }
    }

    *p = nullptr;
    futex = 0;
}

bool DelayedWorker::dispatch(const time_point_t& to, WaitEntry* we, const std::function<void(WaitEntry*)>& wakeup)
{
    bool w = false;
    std::lock_guard<decltype(lock)> l(lock);

    if (futex < 0) {
        return false;
    }

    time_point_t now = std::chrono::steady_clock::now();
    if (to <= now) {
        return false;
    }

    if (map.empty() || to < map.begin()->first) {
        w = true;
    }
    map.emplace(to, DelayedWork {we, &wakeup});
    if (w) {
        futex = 1;
        syscall(SYS_futex, &futex, FUTEX_WAKE, 1);
    }

    return true;
}
} // namespace ffrt