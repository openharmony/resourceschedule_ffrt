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
#include "internal_inc/types.h"
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
        task->coRoutine->blockType = BlockType::BLOCK_THREAD;
        wn->task = task;
    }
    push_back(wn);
    wqlock.unlock();
    {
        std::unique_lock<std::mutex> nl(wn->wl);
        lk->unlock();
        wn->cv.wait(nl);
    }
    wqlock.lock();
    remove(wn);
    wqlock.unlock();
    lk->lock();
}
bool WaitQueue::ThreadWaitUntil(WaitUntilEntry* wn, mutexPrivate* lk,
    const TimePoint& tp, bool legacyMode, CPUEUTask* task)
{
    bool ret = false;
    wqlock.lock();
    if (legacyMode) {
        task->coRoutine->blockType = BlockType::BLOCK_THREAD;
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
    wqlock.lock();
    remove(wn);
    wqlock.unlock();
    lk->lock();
    return ret;
}

void WaitQueue::SuspendAndWait(mutexPrivate* lk)
{
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    CPUEUTask* task = ctx->task;
    bool legacyMode = LegacyMode(task);
    if (!USE_COROUTINE || task == nullptr || legacyMode) {
        ThreadWait(&ctx->wn, lk, legacyMode, task);
        return;
    }
    task->wue = new WaitUntilEntry(task);
    FFRT_BLOCK_TRACER(task->gid, cnd);
    CoWait([&](CPUEUTask* task) -> bool {
        wqlock.lock();
        push_back(task->wue);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        wqlock.unlock();
        return true;
    });
    delete task->wue;
    task->wue = nullptr;
    lk->lock();
}

bool WeTimeoutProc(WaitQueue* wq, WaitUntilEntry* wue)
{
    int expected = we_status::INIT;
    if (!atomic_compare_exchange_strong_explicit(
        &wue->status, &expected, we_status::TIMEOUT, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
        // The critical point wue->status has been written, notify will no longer access wue, it can be deleted
        delete wue;
        return false;
    }

    wq->wqlock.lock();
    if (wue->status.load(std::memory_order_acquire) == we_status::TIMEOUT) {
        wq->remove(wue);
        delete wue;
        wue = nullptr;
    } else {
        wue->status.store(we_status::HANDOVER, std::memory_order_release);
    }
    wq->wqlock.unlock();
    return true;
}

bool WaitQueue::SuspendAndWaitUntil(mutexPrivate* lk, const TimePoint& tp) noexcept
{
    bool ret = false;
    ExecuteCtx* ctx = ExecuteCtx::Cur();
    CPUEUTask* task = ctx->task;
    bool legacyMode = LegacyMode(task);
    if (!USE_COROUTINE || task == nullptr || legacyMode) {
        return ThreadWaitUntil(&ctx->wn, lk, tp, legacyMode, task);
    }
    task->wue = new WaitUntilEntry(task);
    task->wue->hasWaitTime = true;
    task->wue->tp = tp;
    task->wue->cb = ([&](WaitEntry* we) {
        WaitUntilEntry* wue = static_cast<WaitUntilEntry*>(we);
        ffrt::CPUEUTask* task = wue->task;
        if (!WeTimeoutProc(this, wue)) {
            return;
        }
        FFRT_LOGD("task(%d) timeout out", task->gid);
        CoRoutineFactory::CoWakeFunc(task, true);
    });
    FFRT_BLOCK_TRACER(task->gid, cnt);
    CoWait([&](CPUEUTask* task) -> bool {
        WaitUntilEntry* we = task->wue;
        wqlock.lock();
        push_back(we);
        lk->unlock(); // Unlock needs to be in wqlock protection, guaranteed to be executed before lk.lock after CoWake
        wqlock.unlock();
        if (DelayedWakeup(we->tp, we, we->cb)) {
            return true;
        } else {
            if (!WeTimeoutProc(this, we)) {
                return true;
            }
            task->wakeupTimeOut = true;
            return false;
        }
    });
    ret = task->wakeupTimeOut;
    task->wue = nullptr;
    task->wakeupTimeOut = false;
    lk->lock();
    return ret;
}

bool WaitQueue::WeNotifyProc(WaitUntilEntry* we)
{
    if (!we->hasWaitTime) {
        return true;
    }

    auto expected = we_status::INIT;
    if (!atomic_compare_exchange_strong_explicit(
        &we->status, &expected, we_status::NOTIFIED, std::memory_order_seq_cst, std::memory_order_seq_cst)) {
        // The critical point we->status has been written, notify will no longer access we, it can be deleted
        we->status.store(we_status::NOTIFIED, std::memory_order_release);
        wqlock.unlock();
        while (we->status.load(std::memory_order_acquire) != we_status::HANDOVER) {
        }
        delete we;
        wqlock.lock();
        return false;
    }

    return true;
}

void WaitQueue::NotifyOne() noexcept
{
    wqlock.lock();
    while (!empty()) {
        WaitUntilEntry* we = pop_front();
        CPUEUTask* task = we->task;
        bool blockThread = BlockThread(task);
        if (!USE_COROUTINE || we->weType == 2 || blockThread) {
            std::unique_lock<std::mutex> lk(we->wl);
            if (blockThread) {
                task->coRoutine->blockType = BlockType::BLOCK_COROUTINE;
                we->task = nullptr;
            }
            wqlock.unlock();
            we->cv.notify_one();
        } else {
            if (!WeNotifyProc(we)) {
                continue;
            }
            wqlock.unlock();
            CoRoutineFactory::CoWakeFunc(task, false);
        }
        return;
    }
    wqlock.unlock();
}

void WaitQueue::NotifyAll() noexcept
{
    wqlock.lock();
    while (!empty()) {
        WaitUntilEntry* we = pop_front();
        CPUEUTask* task = we->task;
        bool blockThread = BlockThread(task);
        if (!USE_COROUTINE || we->weType == 2 || blockThread) {
            std::unique_lock<std::mutex> lk(we->wl);
            if (blockThread) {
                task->coRoutine->blockType = BlockType::BLOCK_COROUTINE;
                we->task = nullptr;
            }
            wqlock.unlock();
            we->cv.notify_one();
        } else {
            if (!WeNotifyProc(we)) {
                continue;
            }
            wqlock.unlock();
            CoRoutineFactory::CoWakeFunc(task, false);
        }
        wqlock.lock();
    }
    wqlock.unlock();
}
} // namespace ffrt
