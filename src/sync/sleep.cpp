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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <map>
#include <functional>
#include "sync/sync.h"

#include "sched/execute_ctx.h"
#include "eu/co_routine.h"
#include "internal_inc/osal.h"
#include "internal_inc/types.h"
#include "dfx/log/ffrt_log_api.h"
#include "ffrt_trace.h"
#include "tm/cpu_task.h"
#include "cpp/sleep.h"

namespace ffrt {
namespace this_task {
void SleepUntilImpl(const TimePoint& to)
{
    auto task = ExecuteCtx::Cur()->task;
    if (task == nullptr || task->Block() == BlockType::BLOCK_THREAD) {
        std::this_thread::sleep_until(to);
        if (task) {
            task->Wake();
        }
        return;
    }
    // be careful about local-var use-after-free here
    static std::function<void(WaitEntry*)> cb ([](WaitEntry* we) {
        CoRoutineFactory::CoWakeFunc(static_cast<CoTask*>(we->task), CoWakeType::NO_TIMEOUT_WAKE);
    });
    FFRT_BLOCK_TRACER(ExecuteCtx::Cur()->task->gid, slp);
    CoWait([&](CoTask* task) -> bool {
        return DelayedWakeup(to, &task->we, cb); 
    });
}
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif

API_ATTRIBUTE((visibility("default")))
void ffrt_yield()
{
    auto curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask == nullptr || curTask->Block() == ffrt::BlockType::BLOCK_THREAD) {
        std::this_thread::yield();
        if (curTask) {
            curTask->Wake();
        }
        return;
    }
    FFRT_BLOCK_TRACER(curTask->gid, yld);
    CoWait([](ffrt::CoTask* task) -> bool {
        CoRoutineFactory::CoWakeFunc(task, CoWakeType::NO_TIMEOUT_WAKE);
        return true;
    });
}

API_ATTRIBUTE((visibility("default")))
int ffrt_usleep(uint64_t usec)
{
    auto duration = std::chrono::microseconds{usec};
    auto to = std::chrono::steady_clock::now() + duration;

    ffrt::this_task::SleepUntilImpl(to);
    return ffrt_success;
}

#ifdef __cplusplus
}
#endif