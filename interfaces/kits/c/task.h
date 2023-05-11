/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FFRT_API_C_TASK_H
#define FFRT_API_C_TASK_H
#include "type_def.h"

// attr
FFRT_C_API int ffrt_task_attr_init(ffrt_task_attr_t *attr);
FFRT_C_API void ffrt_task_attr_set_name(ffrt_task_attr_t *attr, const char *name);
FFRT_C_API const char *ffrt_task_attr_get_name(const ffrt_task_attr_t *attr);
FFRT_C_API void ffrt_task_attr_destroy(ffrt_task_attr_t *attr);
FFRT_C_API void ffrt_task_attr_set_qos(ffrt_task_attr_t *attr, ffrt_qos_t qos);
FFRT_C_API ffrt_qos_t ffrt_task_attr_get_qos(const ffrt_task_attr_t *attr);
FFRT_C_API void ffrt_task_attr_set_delay(ffrt_task_attr_t *attr, uint64_t delay_us);
FFRT_C_API uint64_t ffrt_task_attr_get_delay(const ffrt_task_attr_t *attr);
FFRT_C_API int ffrt_this_task_update_qos(ffrt_qos_t qos);
FFRT_C_API uint64_t ffrt_this_task_get_id(void);
// deps
FFRT_C_API ffrt_deps_t *ffrt_tls_deps_base(const void *start, const void *arg1, ...);
#define ffrt_deps(...) ffrt_tls_deps_base((const void *)NULL, ##__VA_ARGS__, (const void *)NULL)

// submit
FFRT_C_API void *ffrt_alloc_auto_free_function_storage_base();
FFRT_C_API void ffrt_submit_base(ffrt_function_header_t *f, const ffrt_deps_t *in_deps, const ffrt_deps_t *out_deps,
    const ffrt_task_attr_t *attr);
FFRT_C_API ffrt_task_handle_t ffrt_submit_h_base(ffrt_function_header_t *f, const ffrt_deps_t *in_deps,
    const ffrt_deps_t *out_deps, const ffrt_task_attr_t *attr);
FFRT_C_API void ffrt_task_handle_destroy(ffrt_task_handle_t handle);

// cancel delay_task
FFRT_C_API int ffrt_skip(ffrt_task_handle_t handle);

// wait
FFRT_C_API void ffrt_wait_deps_without_copy_base(const ffrt_deps_t *deps);
FFRT_C_API void ffrt_wait_deps(const ffrt_deps_t *deps);
FFRT_C_API void ffrt_wait();
#endif
