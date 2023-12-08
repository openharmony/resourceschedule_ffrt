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

#ifndef __WAITQUEUE_H__
#define __WAITQUEUE_H__
#include "sync.h"
#include "cpp/mutex.h"
#include "sched/execute_ctx.h"
#include "util/IntrusiveList.h"
#include "sync/mutex_private.h"

namespace ffrt {
class CPUEUTask;
struct TaskWithNode;
using TaskListNode = ListNode;
using TaskList = List<TaskWithNode, TaskListNode>;

struct TaskTimeOutStatus {
    explicit TaskTimeOutStatus(TaskTimeoutState state) : status(state)
    {
    }
    TaskTimeoutState status;
    std::mutex lock;
};

enum class TimeoutState {
    IDLE,
    WAITING,
    TIMEOUTING,
    DONE,
};

struct TimeoutStatus {
    explicit TimeoutStatus(TimeoutState state) : status(state)
    {
    }
    TimeoutState status;
    mutex lock;
};

struct TaskWithNode : public TaskListNode {
    TaskWithNode();
    CPUEUTask* task = nullptr;
    std::mutex lk;
    std::condition_variable cv;
};

class WaitQueue {
public:
    spin_mutex wqlock;
    WaitUntilEntry* whead;
    using TimePoint = std::chrono::steady_clock::time_point;
    void SuspendAndWait(mutexPrivate* lk);
    bool SuspendAndWaitUntil(mutexPrivate* lk, const TimePoint& tp) noexcept;
    bool WeNotifyProc(WaitUntilEntry* we);
    void NotifyAll() noexcept;
    void NotifyOne() noexcept;
    void ThreadWait(WaitUntilEntry* wn, mutexPrivate* lk, bool legacyMode, CPUEUTask* task);
    bool ThreadWaitUntil(WaitUntilEntry* wn, mutexPrivate* lk, const TimePoint& tp, bool legacyMode, CPUEUTask* task);
    WaitQueue()
    {
        whead = new WaitUntilEntry();
        whead->next = whead;
        whead->prev = whead;
    }
    WaitQueue(WaitQueue const&) = delete;
    void operator=(WaitQueue const&) = delete;

    ~WaitQueue()
    {
        wqlock.lock();
        while (!empty()) {
            WaitUntilEntry *wue = pop_front();
            (void)WeNotifyProc(wue);
        }
        wqlock.unlock();
        delete whead;
    }

private:
    inline bool empty() const
    {
        return (whead->next == whead);
    }

    inline void push_back(WaitUntilEntry* we)
    {
        we->next = whead;
        we->prev = whead->prev;
        whead->prev->next = we;
        whead->prev = we;
    }

    inline WaitUntilEntry* pop_front()
    {
        WaitEntry *we = whead->next;
        if (we->next == nullptr) {
            return nullptr;
        }
        whead->next = we->next;
        we->next->prev = whead;
        we->next = nullptr;
        we->prev = nullptr;
        return static_cast<WaitUntilEntry*>(we);
    }

    inline void remove(WaitUntilEntry* we)
    {
        if ((we->next == nullptr) || (we->prev == nullptr)) {
            return;
        }
        we->prev->next = we->next;
        we->next->prev = we->prev;
        we->next = nullptr;
        we->prev = nullptr;
        return;
    }
    friend bool WeTimeoutProc(WaitQueue* wq, WaitUntilEntry* wue);
};
} // namespace ffrt
#endif // _WAITQUEUE_H_
