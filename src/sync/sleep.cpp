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
#include "core/task_ctx.h"
#include "eu/co_routine.h"
#include "internal_inc/osal.h"
#include "internal_inc/types.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/trace/ffrt_trace.h"
#include "cpp/sleep.h"

namespace ffrt {
namespace this_task {
TaskCtx* ExecuteCtxTask()
{
    auto ctx = ExecuteCtx::Cur();
    return ctx->task;
}

void sleep_until_impl(const time_point_t& to)
{
    if (!USE_COROUTINE || ExecuteCtxTask() == nullptr) {
        std::this_thread::sleep_until(to);
        return;
    }
    // be careful about local-var use-after-free here
    std::function<void(WaitEntry*)> cb([](WaitEntry* we) { CoWake(we->task, false); });
    FFRT_BLOCK_TRACER(ExecuteCtxTask()->gid, slp);
    CoWait([&](TaskCtx* inTask) -> bool { return DelayedWakeup(to, &inTask->fq_we, cb); });
}
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif

API_ATTRIBUTE((visibility("default")))
void ffrt_yield()
{
    if (!ffrt::USE_COROUTINE || ffrt::this_task::ExecuteCtxTask() == nullptr) {
        std::this_thread::yield();
        return;
    }
    FFRT_BLOCK_TRACER(ffrt::this_task::ExecuteCtxTask()->gid, yld);
    CoWait([](ffrt::TaskCtx* inTask) -> bool {
        CoWake(inTask, false);
        return true;
    });
}

API_ATTRIBUTE((visibility("default")))
int ffrt_usleep(uint64_t usec)
{
    auto duration = std::chrono::microseconds{usec};
    auto to = std::chrono::steady_clock::now() + duration;

    ffrt::this_task::sleep_until_impl(to);
    return ffrt_success;
}

#ifdef __cplusplus
}
#endif