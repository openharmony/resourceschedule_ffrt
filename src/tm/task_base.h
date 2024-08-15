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

#ifndef _TASK_BASE_H_
#define _TASK_BASE_H_
#include <atomic>
#include <vector>
#include "eu/co_routine.h"
#include "sched/execute_ctx.h"
#include "util/task_deleter.h"

namespace ffrt {
static std::atomic_uint64_t s_gid(0);
class TaskBase {
public:
    uintptr_t reserved = 0;
    uintptr_t type = 0;
    WaitEntry fq_we; // used on fifo fast que
    TaskBase(): gid(++s_gid) {}
    virtual ~TaskBase() = default;
    const uint64_t gid; // global unique id in this process
#ifdef FFRT_ASYNC_STACKTRACE
    uint64_t stackId = 0;
#endif
};

class CoTask : public TaskBase, public TaskDeleter {
public:
    CoTask() = default;
    ~CoTask() override = default;
    virtual void Execute() = 0;

    std::string label;
    std::vector<std::string> traceTag;
    bool wakeupTimeOut = false;
    WaitUntilEntry* wue = nullptr;
    // lifecycle connection between task and coroutine is shown as below:
    // |*task pending*|*task ready*|*task executing*|*task done*|*task release*|
    //                             |**********coroutine*********|
    CoRoutine* coRoutine = nullptr;
    uint64_t stack_size = STACK_SIZE;
    std::atomic<pthread_t> runningTid = 0;
    bool legacyMode { false }; // dynamic switch controlled by set_legacy_mode api
    BlockType blockType { BlockType::BLOCK_COROUTINE }; // block type for lagacy mode changing
    std::mutex mutex_; // used in coroute
    std::condition_variable waitCond_; // cv for thread wait

    void SetTraceTag(const char* name)
    {
        traceTag.emplace_back(name);
    }

    void ClearTraceTag()
    {
        if (!traceTag.empty()) {
            traceTag.pop_back();
        }
    }
};
}
#endif