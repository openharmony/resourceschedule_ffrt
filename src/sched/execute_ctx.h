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

#ifndef FFRT_EXECUTE_CTX_HPP
#define FFRT_EXECUTE_CTX_HPP
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include "util/linked_list.h"
#include "internel_inc/oscl.h"

namespace ffrt {
using time_point_t = std::chrono::steady_clock::time_point;

enum class TaskTimeoutState {
    INIT,
    NOTIFIED,
    TIMEOUT,
};

namespace we_status {
const int INIT = 0;
const int NOTIFIED = 1;
const int TIMEOUT = 2;
} // namespace we_status

struct TaskCtx;

struct WaitEntry {
    WaitEntry() : prev(this), next(this), task(nullptr), weType(0) {
    }
    WaitEntry(TaskCtx *inTask) : prev(nullptr), next(nullptr), task(inTask), weType(0) {
    }
    LinkedList node;
    WaitEntry* prev;
    WaitEntry* next;
    TaskCtx* task;
    int weType;
};

struct WaitUntilEntry : WaitEntry {
    WaitUntilEntry() : WaitEntry(), status(we_status::INIT), hasWaitTime(false)
    {
    }
    WaitUntilEntry(TaskCtx* inTask) : WaitEntry(inTask), status(we_status::INIT), hasWaitTime(false)
    {
    }
    std::atomic_int32_t status;
    bool hasWaitTime;
    time_point_t tp;
    std::function<void(WaitEntry*)> cb;
    std::mutex wl;
    std::condition_variable cv;
};
// 当前Worker线程的状态信息
struct ExecuteCtx {
    ExecuteCtx()
    {
        task = nullptr;
        wn.weType = 2;
    }
    TaskCtx* task; // 当前正在执行的Task
    WaitUntilEntry wn;

    static inline ExecuteCtx* Cur()
    {
        thread_local static ExecuteCtx ctx;
        return &ctx;
    }
};
} // namespace ffrt
#endif
