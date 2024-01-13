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
#include "watchdog_util.h"
#include <sstream>
#include <algorithm>
#include <map>
#include "sync/sync.h"
#include "c/ffrt_watchdog.h"

namespace ffrt {
    static std::map<uint64_t, uint64_t> taskStatusMap;
    static std::mutex lock;

    void AddTaskToWatchdog(uint64_t gid)
    {
        std::lock_guard<decltype(lock)> l(lock);
        taskStatusMap.insert(std::make_pair(gid, gid));
    }

    void RemoveTaskFromWatchdog(uint64_t gid)
    {
        std::lock_guard<decltype(lock)> l(lock);
        taskStatusMap.erase(gid);      
    }

    bool SendTimeoutWatchdog(uint64_t gid, uint64_t timeout, uint64_t delay)
    {
#ifdef FFRT_OH_WATCHDOG_ENABLE
        FFRT_LOGI("start to set watchdog for task gid=%llu with timeout [%llu ms] ", gid, timeout);
        auto now = std::chrono::steady_clock::now();
        WaitUntilEntry* we = new (SimpleAllocator<WaitUntilEntry>::allocMem()) WaitUntilEntry();
        // set dealyedworker callback
        we->cb = ([gid, timeout](WaitEntry* we) {
            std::lock_guard<decltype(lock)> l(lock);
            if (taskStatusMap.count(gid) > 0) {
                RunTimeOutCallback(gid, timeout);
            } else {
                FFRT_LOGI("task gid=%llu has finished", gid);                
            }
            SimpleAllocator<WaitUntilEntry>::freeMem(static_cast<WaitUntilEntry*>(we));
        });
        // set dealyedworker wakeup time
        std::chrono::microseconds timeoutTime(timeout * 1000);
        std::chrono::microseconds delayTime(delay);
        we->tp = (now + timeoutTime + delayTime);
        if (!DelayedWakeup(we->tp, we, we->cb)) {
            SimpleAllocator<WaitUntilEntry>::freeMem(we);
            FFRT_LOGE("failed to set watchdog for task gid=%llu with timeout [%llu ms] ", gid, timeout);
            return false;
        }
#endif
        return true;
    }

    void RunTimeOutCallback(uint64_t gid, uint64_t timeout)
    {
#ifdef FFRT_OH_WATCHDOG_ENABLE
    std::stringstream ss;
    ss << "parallel task gid=" << gid << " execution time exceeds " << timeout << " us";
    std::string msg = ss.str();
    FFRT_LOGE("%s", msg.c_str());
    ffrt_watchdog_cb func = ffrt_watchdog_get_cb();
    if (func) {
        func(gid, msg.c_str(), msg.size());
    }
    if (!SendTimeoutWatchdog(gid, timeout, 0)) {
        FFRT_LOGE("parallel task gid=%llu send next watchdog delaywork failed", gid);        
    }
#endif
    }
}
