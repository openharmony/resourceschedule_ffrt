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
#ifndef FFRT_WATCHDOG_H
#define FFRT_WATCHDOG_H
#include <stdint.h>
#include "type_def_ext.h"

typedef void(*ffrt_watchdog_cb)(uint64_t, const char *, uint32_t);
FFRT_C_API int ffrt_watchdog_dumpinfo(char *buf, uint32_t len);
FFRT_C_API void ffrt_watchdog_register(ffrt_watchdog_cb cb, uint32_t timeout_ms, uint32_t interval_ms);
FFRT_C_API ffrt_watchdog_cb ffrt_watchdog_get_cb(void);
FFRT_C_API uint32_t ffrt_watchdog_get_timeout(void);
FFRT_C_API uint32_t ffrt_watchdog_get_interval(void);
#endif /* FFRT_WATCHDOG_H */