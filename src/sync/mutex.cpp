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

#include <unistd.h>
#include "cpp/mutex.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <map>
#include <functional>
#include "sync/sync.h"
#include "core/task_ctx.h"
#include "eu/co_routine.h"
#include "internal_inc/osal.h"
#include "sync/mutex_private.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {

bool mutexPrivate::try_lock()
{
    int v = sync_detail::UNLOCK;
    return l.compare_exchange_strong(v, sync_detail::LOCK, std::memory_order_acquire, std::memory_order_relaxed);
}

void mutexPrivate::lock()
{
    int v = sync_detail::UNLOCK;
    if (l.compare_exchange_strong(v, sync_detail::LOCK, std::memory_order_acquire, std::memory_order_relaxed)) {
        return;
    }
    if (l.load(std::memory_order_relaxed) == sync_detail::WAIT) {
        wait();
    }
    while (l.exchange(sync_detail::WAIT, std::memory_order_acquire) != sync_detail::UNLOCK) {
        wait();
    }
}

void mutexPrivate::unlock()
{
    if (l.exchange(sync_detail::UNLOCK, std::memory_order_release) == sync_detail::WAIT) {
        wake();
    }
}

void mutexPrivate::wait()
{
    auto ctx = ExecuteCtx::Cur();
    auto task = ctx->task;
    if (task == nullptr) {
        wlock.lock();
        if (l.load(std::memory_order_relaxed) != sync_detail::WAIT) {
            wlock.unlock();
            return;
        }
        list.PushBack(ctx->wn.node);
        std::unique_lock<std::mutex> lk(ctx->wn.wl);
        wlock.unlock();
        ctx->wn.cv.wait(lk);
        return;
    } else {
        CoWait([this](TaskCtx* inTask) -> bool {
            wlock.lock();
            if (l.load(std::memory_order_relaxed) != sync_detail::WAIT) {
                wlock.unlock();
                return false;
            }
            list.PushBack(inTask->fq_we.node);
            wlock.unlock();
            return true;
        });
    }
}
void mutexPrivate::wake()
{
    wlock.lock();
    if (list.Empty()) {
        wlock.unlock();
        return;
    }
    WaitEntry* we = list.PopFront(&WaitEntry::node);
    TaskCtx* task = we->task;
    if (we->weType == 2) {
        WaitUntilEntry* wue = static_cast<WaitUntilEntry*>(we);
        std::unique_lock lk(wue->wl);
        wlock.unlock();
        wue->cv.notify_one();
    } else {
        wlock.unlock();
        CoWake(task, false);
    }
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_mtx_init(ffrt_mtx_t *mutex, int type)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_thrd_error;
    }
    if (!(type == ffrt_mtx_plain)) {
        assert(false);
    }
    static_assert(sizeof(ffrt::mutexPrivate) <= ffrt_mutex_storage_size,
        "size must be less than ffrt_mutex_storage_size");

    new (mutex)ffrt::mutexPrivate();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mtx_lock(ffrt_mtx_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_thrd_error;
    }
    auto p = (ffrt::mutexPrivate*)mutex;
    p->lock();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mtx_unlock(ffrt_mtx_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_thrd_error;
    }
    auto p = (ffrt::mutexPrivate*)mutex;
    p->unlock();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_mtx_trylock(ffrt_mtx_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return ffrt_thrd_error;
    }
    auto p = (ffrt::mutexPrivate*)mutex;
    return p->try_lock() ? ffrt_thrd_success : ffrt_thrd_busy;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_mtx_destroy(ffrt_mtx_t* mutex)
{
    if (!mutex) {
        FFRT_LOGE("mutex should not be empty");
        return;
    }
    auto p = (ffrt::mutexPrivate*)mutex;
    p->~mutexPrivate();
}
#ifdef __cplusplus
}
#endif
