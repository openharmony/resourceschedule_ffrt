/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#ifndef FFRT_DYNAMIC_GRAPH_C_API_H
#define FFRT_DYNAMIC_GRAPH_C_API_H
#include "ffrt_types.h"
#include "c/type_def.h"

FFRT_C_API ffrt_task_handle_t ffrt_hcs_submit_h(const ffrt_hcs_task_t *task, const ffrt_deps_t *in_deps,
    const ffrt_deps_t *out_deps, const ffrt_task_attr_t *attr);

FFRT_C_API int ffrt_hcs_wait_deps(const ffrt_deps_t *deps);
FFRT_C_API void ffrt_hcs_wait();

FFRT_C_API bool ffrt_hcs_get_capability(uint32_t support_bitmap);

#endif // FFRT_DYNAMIC_GRAPH_C_API_H