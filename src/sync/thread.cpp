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

#include "cpp/thread.h"
#include "c/thread.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_thrd_create(ffrt_thrd_t* thr, ffrt_thrd_start_t func, void* arg)
{
    if (!thr || !func) {
        FFRT_LOGE("thr and func should not be empty");
        return ffrt_thrd_error;
    }
    *thr = new ffrt::thread(func, arg);
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_thrd_join(ffrt_thrd_t* thr)
{
    if (!thr) {
        FFRT_LOGE("thr should not be empty");
        return ffrt_thrd_error;
    }
    auto p = static_cast<ffrt::thread*>(*thr);
    p->join();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_thrd_detach(ffrt_thrd_t* thr)
{
    if (!thr) {
        FFRT_LOGE("thr should not be empty");
        return ffrt_thrd_error;
    }
    auto p = static_cast<ffrt::thread*>(*thr);
    p->detach();
    return ffrt_thrd_success;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_thrd_exit(ffrt_thrd_t* thr)
{
    if (!thr) {
        FFRT_LOGE("thr should not be empty");
        return;
    }
    delete static_cast<ffrt::thread*>(*thr);
}
#ifdef __cplusplus
}
#endif
