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

#include "cpp/condition_variable.h"
#include "c/condition_variable.h"
#include "sync/wait_queue.h"
#include "sync/mutex_private.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
using condition_variable_private = WaitQueue;
}

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_cnd_init(ffrt_cnd_t* cnd)
{
    if (!cnd) {
        FFRT_LOGE("cnd should not be empty");
        return ffrt_thrd_error;
    }
    static_assert(sizeof(ffrt::condition_variable_private) <= ffrt_cnd_storage_size,
        "size must be less than ffrt_cnd_storage_size");

    new (cnd) ffrt::condition_variable_private();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_cnd_signal(ffrt_cnd_t* cnd)
{
    if (!cnd) {
        FFRT_LOGE("cnd should not be empty");
        return ffrt_thrd_error;
    }
    auto p = (ffrt::condition_variable_private*)cnd;
    p->NotifyOne();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_cnd_broadcast(ffrt_cnd_t* cnd)
{
    if (!cnd) {
        FFRT_LOGE("cnd should not be empty");
        return ffrt_thrd_error;
    }
    auto p = (ffrt::condition_variable_private*)cnd;
    p->NotifyAll();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_cnd_wait(ffrt_cnd_t* cnd, ffrt_mtx_t* mutex)
{
    if (!cnd || !mutex) {
        FFRT_LOGE("cnd and mutex should not be empty");
        return ffrt_thrd_error;
    }
    auto pc = (ffrt::condition_variable_private*)cnd;
    auto pm = (ffrt::mutexPrivate*)mutex;
    pc->SuspendAndWait(pm);
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_cnd_timedwait(ffrt_cnd_t* cnd, ffrt_mtx_t* mutex, const struct timespec* time_point)
{
    if (!cnd || !mutex || !time_point) {
        FFRT_LOGE("cnd, mutex and time_point should not be empty");
        return ffrt_thrd_error;
    }
    auto pc = (ffrt::condition_variable_private*)cnd;
    auto pm = (ffrt::mutexPrivate*)mutex;

    using namespace std::chrono;
    auto duration = seconds{time_point->tv_sec} + nanoseconds{time_point->tv_nsec};
    auto tp = ffrt::time_point_t {
        duration_cast<steady_clock::duration>(duration_cast<nanoseconds>(duration))
    };

    return pc->SuspendAndWaitUntil(pm, tp) ? ffrt_thrd_timedout : ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_cnd_destroy(ffrt_cnd_t* cnd)
{
    if (!cnd) {
        FFRT_LOGE("cnd should not be empty");
        return;
    }
    auto p = (ffrt::condition_variable_private*)cnd;
    p->~WaitQueue();
}
#ifdef __cplusplus
}
#endif
