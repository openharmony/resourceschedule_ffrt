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
CPUEUTask* ExecuteCtxTask()
{
    auto ctx = ExecuteCtx::Cur();
    return ctx->task;
}

void SleepUntilImpl(const TimePoint& to)
{
    auto task = ExecuteCtxTask();
    if (ThreadWaitMode(task)) {
        if (FFRT_UNLIKELY(LegacyMode(task))) {
            task->blockType = BlockType::BLOCK_THREAD;
        }
        std::this_thread::sleep_until(to);
        if (BlockThread(task)) {
            task->blockType = BlockType::BLOCK_COROUTINE;
        }
        return;
    }
    // be careful about local-var use-after-free here
    static std::function<void(WaitEntry*)> cb ([](WaitEntry* we) {
        CoRoutineFactory::CoWakeFunc(we->task, CoWakeType::NO_TIMEOUT_WAKE);
    });
    FFRT_BLOCK_TRACER(ExecuteCtxTask()->gid, slp);
    CoWait([&](CPUEUTask* task) -> bool { return DelayedWakeup(to, &task->fq_we, cb); });
}
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif

API_ATTRIBUTE((visibility("default")))
void ffrt_yield()
{
    auto curTask = ffrt::this_task::ExecuteCtxTask();
    if (ThreadWaitMode(curTask)) {
        if (FFRT_UNLIKELY(LegacyMode(curTask))) {
            curTask->blockType = BlockType::BLOCK_THREAD;
        }
        std::this_thread::yield();
        if (BlockThread(curTask)) {
            curTask->blockType = BlockType::BLOCK_COROUTINE;
        }
        return;
    }
    FFRT_BLOCK_TRACER(curTask->gid, yld);
    CoWait([](ffrt::CPUEUTask* task) -> bool {
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