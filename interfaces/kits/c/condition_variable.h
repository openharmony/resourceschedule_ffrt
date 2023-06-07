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
#ifndef FFRT_API_C_CONDITION_VARIABLE_H
#define FFRT_API_C_CONDITION_VARIABLE_H
#include <time.h>
#include "type_def.h"

typedef enum {
    ffrt_clock_realtime = CLOCK_REALTIME,
    ffrt_clock_monotonic = CLOCK_MONOTONIC
} ffrt_clockid_t;

FFRT_C_API int ffrt_condattr_init(ffrt_condattr_t* attr);
FFRT_C_API int ffrt_condattr_destroy(ffrt_condattr_t* attr);
FFRT_C_API int ffrt_condattr_setclock(ffrt_condattr_t* attr, ffrt_clockid_t clock);
FFRT_C_API int ffrt_condattr_getclock(const ffrt_condattr_t* attr, ffrt_clockid_t* clock);

FFRT_C_API int ffrt_cond_init(ffrt_cond_t* cond, const ffrt_condattr_t* attr);
FFRT_C_API int ffrt_cond_signal(ffrt_cond_t* cond);
FFRT_C_API int ffrt_cond_broadcast(ffrt_cond_t* cond);
FFRT_C_API int ffrt_cond_wait(ffrt_cond_t* cond, ffrt_mutex_t* mutex);
FFRT_C_API int ffrt_cond_timedwait(ffrt_cond_t* cond, ffrt_mutex_t* mutex, const struct timespec* time_point);
FFRT_C_API int ffrt_cond_destroy(ffrt_cond_t* cond);
#endif
