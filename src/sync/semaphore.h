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

#ifndef SYNC_SEMAPHORE_H
#define SYNC_SEMAPHORE_H

#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include "sched/execute_ctx.h"
#include "delayed_worker.h"

namespace ffrt {
class semaphore {
    uint64_t data;

public:
    semaphore() : data(0)
    {
    }
    semaphore(uint32_t d) : data(d)
    {
    }
    semaphore(semaphore const&) = delete;
    void operator=(semaphore const&) = delete;

    void acquire()
    {
        const uint64_t w = static_cast<uint64_t>(1) << 32;

        uint64_t d = __atomic_fetch_add(&data, w, __ATOMIC_RELAXED);
        for (;;) {
            // RunnableTaskNumber == 0 ?
            if (static_cast<uint32_t>(d) == 0) {
                // RQ is empty, then sleep
                syscall(SYS_futex, &data, FUTEX_WAIT_PRIVATE, 0, nullptr, nullptr, 0);
                d = __atomic_load_n(&data, __ATOMIC_RELAXED);
            } else if (__atomic_compare_exchange_n(&data, &d, d - 1 - w, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                break;
            }
        }
    }

    void release()
    {
        // increase RunnableTaskNumber
        uint64_t d = __atomic_fetch_add(&data, 1, __ATOMIC_RELEASE);
        // overflow
        if (static_cast<uint32_t>(d) == ~0u) {
            abort();
        }
        // if RunnableTaskNumber == 0 && SleepWorkerNumber > 0, then wake up sleep worker
        // if RQ is empty before new task is enqueued, RunnableTaskNumber is from 0 to 1
        if (static_cast<uint32_t>(d) == 0 && (d >> 32) > 0) {
            syscall(SYS_futex, &data, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
        }

        // if RunnableTaskNumber >= 4 && SleepWorkerNumber > 0 , then wake up another sleep worker
        if (static_cast<uint32_t>(d) >= 4 && (d >> 32) > 0) {
            syscall(SYS_futex, &data, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
        }
    }
};
} // namespace ffrt
#endif
