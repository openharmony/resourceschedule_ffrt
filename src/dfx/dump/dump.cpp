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
#include "c/ffrt_dump.h"
#include "c/ffrt_watchdog.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_dump(uint32_t cmd, char *buf, uint32_t len)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    switch (static_cast<ffrt_dump_cmd_t>(cmd)) {
        case ffrt_dump_cmd_t::DUMP_INFO_ALL: {
            return ffrt_watchdog_dumpinfo(buf, len);
        }
        default: {
            FFRT_LOGE("ffr_dump unsupport cmd[%d]", cmd);
            return -1;
        }
    }
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}
#ifdef __cplusplus
}
#endif