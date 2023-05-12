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

FFRT_C_API int ffrt_cnd_init(ffrt_cnd_t* cnd);
FFRT_C_API int ffrt_cnd_signal(ffrt_cnd_t* cnd);
FFRT_C_API int ffrt_cnd_broadcast(ffrt_cnd_t* cnd);
FFRT_C_API int ffrt_cnd_wait(ffrt_cnd_t* cnd, ffrt_mtx_t* mutex);
FFRT_C_API int ffrt_cnd_timedwait(ffrt_cnd_t* cnd, ffrt_mtx_t* mutex, const struct timespec* time_point);
FFRT_C_API void ffrt_cnd_destroy(ffrt_cnd_t* cnd);
#endif
