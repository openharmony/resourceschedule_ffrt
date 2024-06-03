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
#ifndef FFRT_API_C_FFRT_DUMP_H
#define FFRT_API_C_FFRT_DUMP_H
#include <stdint.h>
#include "type_def_ext.h"

typedef enum {
    DUMP_INFO_ALL = 0,
} ffrt_dump_cmd_t;

typedef void(*ffrt_task_timeout_cb)(uint64_t, const char *, uint32_t);

FFRT_C_API int ffrt_dump(uint32_t cmd, char *buf, uint32_t len);
FFRT_C_API ffrt_task_timeout_cb ffrt_task_timeout_get_cb(void);
FFRT_C_API void ffrt_task_timeout_set_cb(ffrt_task_timeout_cb cb);
FFRT_C_API uint32_t ffrt_task_timeout_get_threshold(void);
FFRT_C_API void ffrt_task_timeout_set_threshold(uint32_t threshold_ms);
#endif /* FFRT_API_C_FFRT_DUMP_H */