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

#include "internal_inc/osal.h"
#include "util/linked_list.h"
#include "c/executor_task.h"
#include "util/spmc_queue.h"
#ifdef USE_OHOS_QOS
#include "qos.h"
#else
#include "staging_qos/sched/qos.h"
#endif

namespace ffrt {
using TimePoint = std::chrono::steady_clock::time_point;

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

enum class ThreadType {
    USER_THREAD, // 非FFRT的线程，属于用户线程
    FFRT_WORKER, // 执行ACPU任务的FFRT线程
    DELAY_WORKER, // 执行定时和监控事件的FFRT线程
    IO_POLLER // 执行fd监听的FFRT线程
};

enum class WaitEntryStatus {
    INIT,
    NOTIFYING,
    TIMEOUT_DONE,
}; // namespace WaitEntryStatus

class TaskBase;
class CoTask;

struct WaitEntry {
    WaitEntry() : prev(this), next(this), task(nullptr), wtType(SharedMutexWaitType::NORMAL) {
    }
    explicit WaitEntry(TaskBase *task) : prev(nullptr), next(nullptr), task(task),
        wtType(SharedMutexWaitType::NORMAL) {
    }
    LinkedList node;
    WaitEntry* prev;
    WaitEntry* next;
    TaskBase* task;
    SharedMutexWaitType wtType;
};

struct WaitUntilEntry : WaitEntry {
    WaitUntilEntry() : WaitEntry(), status(WaitEntryStatus::INIT), hasWaitTime(false)
    {
    }
    explicit WaitUntilEntry(TaskBase* task) : WaitEntry(task), status(WaitEntryStatus::INIT), hasWaitTime(false)
    {
    }
    std::atomic<WaitEntryStatus> status;
    bool hasWaitTime;
    TimePoint tp;
    std::function<void(WaitEntry*)> cb;
    std::mutex wl;
    std::condition_variable cv;
};
// 当前Worker线程的状态信息
struct ExecuteCtx {
    ExecuteCtx();
    virtual ~ExecuteCtx();

    QoS qos;
    TaskBase* task; // 当前正在执行的Task
    WaitUntilEntry wn;
    uint64_t lastGid_ = 0;
    pid_t tid;
    ThreadType threadType_ = ffrt::ThreadType::USER_THREAD;

    static inline pthread_key_t executeCtxTlsKey_ = 0;
    static inline pthread_once_t executeCtxKeyOnce_ = PTHREAD_ONCE_INIT;

    static FFRT_NOINLINE void CreateExecuteCtx()
    {
        void* ctx = new ExecuteCtx();
        pthread_setspecific(ExecuteCtx::executeCtxTlsKey_, ctx);
    }

    /**
     * @param init Should ExecuteCtx be initialized if it cannot be obtained
     */
    template<bool init = true>
    static FFRT_INLINE ExecuteCtx* Cur()
    {
        void* ctx = pthread_getspecific(ExecuteCtx::executeCtxTlsKey_);
        if constexpr (init) {
            if unlikely(ctx == nullptr) {
                CreateExecuteCtx();
            }
        }
        return reinterpret_cast<ExecuteCtx*>(ctx);
    }

    static ExecuteCtx* Cur(bool init = true);
    static void CtxEnvCreate();
};
} // namespace ffrt
#endif
