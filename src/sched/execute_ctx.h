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
#ifdef FFRT_IO_TASK_SCHEDULER
#include "c/executor_task.h"
#include "util/spmc_queue.h"
#ifdef USE_OHOS_QOS
#include "qos.h"
#else
#include "staging_qos/sched/qos.h"
#endif
#endif

namespace ffrt {
using time_point_t = std::chrono::steady_clock::time_point;

enum class TaskTimeoutState {
    INIT,
    NOTIFIED,
    TIMEOUT,
};

enum class SharedMutexWaitType {
    NORMAL,
    READ,
    WRITE,
};

namespace we_status {
const int INIT = 0;
const int NOTIFIED = 1;
const int TIMEOUT = 2;
const int HANDOVER = 3;
} // namespace we_status

class CPUEUTask;

struct WaitEntry {
    WaitEntry() : prev(this), next(this), task(nullptr), weType(0), wtType(SharedMutexWaitType::NORMAL) {
    }
    explicit WaitEntry(CPUEUTask *task) : prev(nullptr), next(nullptr), task(task), weType(0),
        wtType(SharedMutexWaitType::NORMAL) {
    }
    LinkedList node;
    WaitEntry* prev;
    WaitEntry* next;
    CPUEUTask* task;
    int weType;
    SharedMutexWaitType wtType;
};

struct WaitUntilEntry : WaitEntry {
    WaitUntilEntry() : WaitEntry(), status(we_status::INIT), hasWaitTime(false)
    {
    }
    explicit WaitUntilEntry(CPUEUTask* task) : WaitEntry(task), status(we_status::INIT), hasWaitTime(false)
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
    ExecuteCtx();
    virtual ~ExecuteCtx();

#ifdef FFRT_IO_TASK_SCHEDULER
    ffrt_executor_task_t* exec_task = nullptr;
    void** priority_task_ptr = nullptr;
    SpmcQueue* localFifo = nullptr;
    QoS qos;
#endif
    CPUEUTask* task; // 当前正在执行的Task
    WaitUntilEntry wn;

#ifdef FFRT_IO_TASK_SCHEDULER
    inline bool PushTaskToPriorityStack(ffrt_executor_task_t* task)
    {
        if (priority_task_ptr == nullptr) {
            return false;
        }
        if (*priority_task_ptr == nullptr) {
            *priority_task_ptr = task;
            return true;
        }
        return false;
    }
#endif

    static ExecuteCtx* Cur();
};
} // namespace ffrt
#endif
