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
#ifndef FFRT_API_C_THREAD_H
#define FFRT_API_C_THREAD_H
#include "type_def.h"

typedef int(*ffrt_thrd_start_t)(void*);
FFRT_C_API int ffrt_thrd_create(ffrt_thrd_t* thr, ffrt_thrd_start_t func, void* arg);
FFRT_C_API int ffrt_thrd_join(ffrt_thrd_t* thr);
FFRT_C_API int ffrt_thrd_detach(ffrt_thrd_t* thr);
FFRT_C_API void ffrt_thrd_exit(ffrt_thrd_t* thr);
#endif
