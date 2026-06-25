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

#include <unistd.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/time.h>
#include <sys/syscall.h>

#include <map>
#include <functional>
#include <linux/futex.h>
#include "sync/delayed_worker.h"
#include "util/ffrt_facade.h"
#include "sync/sync.h"

#ifdef NS_PER_SEC
#undef NS_PER_SEC
#endif
namespace ffrt {
bool DelayedWakeup(const TimePoint& to, WaitEntry* we, const std::function<void(WaitEntry*)>& wakeup,
    bool skipTimeCheck)
{
    return FFRTFacade::GetDelayedWorker().dispatch(to, we, wakeup, skipTimeCheck);
}

bool DelayedRemove(const TimePoint& to, WaitEntry* we)
{
    return FFRTFacade::GetDelayedWorker().remove(to, we);
}

void spin_mutex::lock_contended()
{
    int v = l.load(std::memory_order_relaxed);
    do {
        while (v != sync_detail::UNLOCK) {
            std::this_thread::yield();
            v = l.load(std::memory_order_relaxed);
        }
    } while (!l.compare_exchange_weak(v, sync_detail::LOCK, std::memory_order_acquire, std::memory_order_relaxed));
}
} // namespace ffrt