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
#include "ffrt_watchdog.h"
#include "internal_inc/osal.h"
#include "dfx/bbox/bbox.h"

namespace ffrt {
constexpr uint32_t DEFAULT_TIMEOUT_MS = 30000;
struct WatchdogCfg {
    static inline WatchdogCfg* Instance()
    {
        static WatchdogCfg inst;
        return &inst;
    }

    uint32_t timeout = DEFAULT_TIMEOUT_MS;
    uint32_t interval = DEFAULT_TIMEOUT_MS;
    ffrt_watchdog_cb callback = nullptr;
};
}

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
void ffrt_watchdog_dumpinfo(char *buf, uint32_t len)
{
    if (FFRTIsWork()) {
        std::string dumpInfo;
        dumpInfo += SaveTaskCounterInfo();
        dumpInfo += SaveWorkerStatusInfo();
        dumpInfo += SaveReadyQueueStatusInfo();
        dumpInfo += SaveTaskStatusInfo();
        snprintf(buf, len, "%s", dumpInfo.c_str());
    } else {
        snprintf(buf, len, "%s", "FFRT has done all tasks!");
    }
}

API_ATTRIBUTE((visibility("default")))
void ffrt_watchdog_register(ffrt_watchdog_cb cb, uint32_t timeout_ms, uint32_t interval_ms)
{
    ffrt::WatchdogCfg::Instance()->callback = cb;
    ffrt::WatchdogCfg::Instance()->timeout = timeout_ms;
    ffrt::WatchdogCfg::Instance()->interval = interval_ms;
}

API_ATTRIBUTE((visibility("default")))
ffrt_watchdog_cb ffrt_watchdog_get_cb(void)
{
    return ffrt::WatchdogCfg::Instance()->callback;
}

API_ATTRIBUTE((visibility("default")))
uint32_t ffrt_watchdog_get_timeout(void)
{
    return ffrt::WatchdogCfg::Instance()->timeout;
}

API_ATTRIBUTE((visibility("default")))
uint32_t ffrt_watchdog_get_interval(void)
{
    return ffrt::WatchdogCfg::Instance()->interval;
}
#ifdef __cplusplus
}
#endif
