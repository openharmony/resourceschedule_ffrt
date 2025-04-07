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

#include "wait_queue.h"
#include "sched/execute_ctx.h"
#include "eu/co_routine.h"
#include "dfx/log/ffrt_log_api.h"
#include "ffrt_trace.h"
#include "sync/mutex_private.h"
#include "tm/cpu_task.h"

namespace ffrt {
TaskWithNode::TaskWithNode()
{
    auto ctx = ExecuteCtx::Cur();
    task = ctx->task;
}

void WaitQueue::ThreadWait(WaitUntilEntry* wn, mutexPrivate* lk, bool legacyMode, CPUEUTask* task)
{
    wqlock.lock();
    if (legacyMode) {
        task->blockType = BlockType::BLOCK_THREAD;
        wn->task = task;
    }
    push_back(wn);
    wqlock.unlock();
    {
        std::unique_lock<std::mutex> nl(wn->wl);
        lk->unlock();
        wn->cv.wait(nl);
    }
    wn->task = nullptr;
    lk->lock();
}

bool WaitQueue::ThreadWaitUntil(WaitUntilEntry* wn, mutexPrivate* lk,
    const TimePoint& tp, bool legacyMode, CPUEUTask* task)
{
    bool ret = false;
    wqlock.lock();
    wn->status.store(we_status::INIT, std::memory_order_release);
    if (legacyMode) {
        task->blockType = BlockType::BLOCK_THREAD;
        wn->task = task;
    }
    push_back(wn);
    wqlock.unlock();
    {
        std::unique_lock<std::mutex> nl(wn->wl);
        lk->unlock();
        if (wn->cv.wait_until(nl, tp) == std::cv_status::timeout) {
            ret = true;
        }
    }
    wn->task = nullptr;

    // notify scenarios wn is already pooped
    // in addition, condition variables may be spurious woken up
    // in this case, wn needs to be removed from the linked list
    if (ret || wn->status.load(std::memory_order_acquire) != we_status::NOTIFING) {
        wqlock.lock();
        remove(wn);
        wqlock.unlock();
    }
    lk->lock();
    return ret;
}

void WaitQueue::SuspendAndWait(mutexPrivate* lk)
{
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    CPUEUTask* task = ctx->task;
    if (ThreadWaitMode(task)) {
        ThreadWait(&ctx->wn, lk, LegacyMode(task), task);
        return;
    }
    task->wue = new (std::nothrow) WaitUntilEntry(task);
    FFRT_COND_RETURN_VOID(task->wue == nullptr, "new WaitUntilEntry failed");
    FFRT_BLOCK_TRACER(task->gid, cnd);
    CoWait([&](CPUEUTask* task) -> bool {
        wqlock.lock();
        push_back(task->wue);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        wqlock.unlock();
        // The ownership of the task belongs to WaitQueue list, and the task cannot be accessed anymore.
        return true;
    });
    delete task->wue;
    task->wue = nullptr;
    lk->lock();
}

bool WeTimeoutProc(WaitQueue* wq, WaitUntilEntry* wue)
{
    bool toWake = true;

    // two kinds: 1) notify was not called, timeout grabbed the lock first;
    if (wue->status.load(std::memory_order_acquire) == we_status::INIT) {
        // timeout processes wue first, cv will not be processed again. timeout is responsible for destroying wue.
        wq->remove(wue);
        delete wue;
        wue = nullptr;
    } else {
        // 2) notify enters the critical section, first writes the notify status, and then releases the lock
        // notify is responsible for destroying wue.
        wue->status.store(we_status::TIMEOUT_DONE, std::memory_order_release);
        toWake = false;
    }
    return toWake;
}

int WaitQueue::SuspendAndWaitUntil(mutexPrivate* lk, const TimePoint& tp) noexcept
{
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    CPUEUTask* task = ctx->task;
    int ret = ffrt_success;
    if (ThreadWaitMode(task)) {
        ret = ThreadWaitUntil(&ctx->wn, lk, tp, LegacyMode(task), task) ? ffrt_error_timedout : ffrt_success;
        return ret;
    }
    task->wue = new WaitUntilEntry(task);
    task->wue->hasWaitTime = true;
    task->wue->tp = tp;
    task->wue->cb = ([&](WaitEntry* we) {
        WaitUntilEntry* wue = static_cast<WaitUntilEntry*>(we);
        ffrt::CPUEUTask* task = wue->task;
        wqlock.lock();
        if (!WeTimeoutProc(this, wue)) {
            wqlock.unlock();
            return;
        }
        wqlock.unlock();
        FFRT_LOGD("task(%d) time is up", task->gid);
        CoRoutineFactory::CoWakeFunc(task, CoWakeType::TIMEOUT_WAKE);
    });
    FFRT_BLOCK_TRACER(task->gid, cnt);
    CoWait([&](CPUEUTask* task) -> bool {
        WaitUntilEntry* we = task->wue;
        wqlock.lock();
        push_back(we);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        if (DelayedWakeup(we->tp, we, we->cb)) {
            wqlock.unlock();
            // The ownership of the task belongs to WaitQueue list, and the task cannot be accessed anymore.
            return true;
        } else {
            if (!WeTimeoutProc(this, we)) {
                wqlock.unlock();
                // The ownership of the task belongs to WaitQueue list, and the task cannot be accessed anymore.
                return true;
            }
            task->coWakeType = CoWakeType::TIMEOUT_WAKE;
            wqlock.unlock();
            // The ownership of the task belongs to WaitQueue list, and the task cannot be accessed anymore.
            return false;
        }
    });
    ret = task->coWakeType == CoWakeType::NO_TIMEOUT_WAKE ? ffrt_success : ffrt_error_timedout;
    task->wue = nullptr;
    task->coWakeType = CoWakeType::NO_TIMEOUT_WAKE;
    lk->lock();
    return ret;
}

void WaitQueue::WeNotifyProc(WaitUntilEntry* we)
{
    if (!we->hasWaitTime) {
        // For wait task without timeout, we will be deleted after the wait task wakes up.
        return;
    }

    WaitEntry* dwe = static_cast<WaitEntry*>(we);
    if (!DelayedRemove(we->tp, dwe)) {
        // Deletion of timer failed during the notify process, indicating that timer cb has been executed at this time
        // waiting for cb execution to complete, and marking notify as being processed.
        we->status.store(we_status::NOTIFING, std::memory_order_release);
        wqlock.unlock();
        while (we->status.load(std::memory_order_acquire) != we_status::TIMEOUT_DONE) {
        }
        wqlock.lock();
    }
    delete we;
}

void WaitQueue::Notify(bool one) noexcept
{
    // the caller should assure the WaitQueue lifetime.
    // this function should assure the WaitQueue do not be access after the wqlock is empty(),
    // that mean the last wait thread/co may destroy the WaitQueue.
    // all the break-out should assure the wqlock is in unlock state.
    // the continue should assure the wqlock is in lock state.
    wqlock.lock();
    for (; ;) {
        if (empty()) {
            wqlock.unlock();
            break;
        }
        WaitUntilEntry* we = pop_front();
        if (we == nullptr) {
            wqlock.unlock();
            break;
        }
        bool isEmpty = empty();
        CPUEUTask* task = we->task;
        if (ThreadNotifyMode(task) || we->weType == 2) {
            std::unique_lock<std::mutex> lk(we->wl);
            we->status.store(we_status::NOTIFING, std::memory_order_release);
            if (BlockThread(task)) {
                task->blockType = BlockType::BLOCK_COROUTINE;
                we->task = nullptr;
            }
            wqlock.unlock();
            we->cv.notify_one();
        } else {
            WeNotifyProc(we);
            wqlock.unlock();
            CoRoutineFactory::CoWakeFunc(task, CoWakeType::NO_TIMEOUT_WAKE);
        }
        if (isEmpty || one) {
            break;
        }
        wqlock.lock();
    }
}

} // namespace ffrt
