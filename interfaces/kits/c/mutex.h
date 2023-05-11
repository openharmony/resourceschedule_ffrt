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
#ifndef FFRT_API_C_MUTEX_H
#define FFRT_API_C_MUTEX_H
#include "type_def.h"

FFRT_C_API int ffrt_mtx_init(ffrt_mtx_t* mutex, int type);
FFRT_C_API int ffrt_mtx_lock(ffrt_mtx_t* mutex);
FFRT_C_API int ffrt_mtx_unlock(ffrt_mtx_t* mutex);
FFRT_C_API int ffrt_mtx_trylock(ffrt_mtx_t* mutex);
FFRT_C_API void ffrt_mtx_destroy(ffrt_mtx_t* mutex);
#endif
