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
#include "c/ffrt_watchdog.h"
#include "internal_inc/osal.h"
#include "dfx/bbox/bbox.h"

namespace ffrt {
struct WatchdogCfg {
public:
    explicit WatchdogCfg(ffrt_watchdog_cb* cb, uint32_t timeout_ms = 30000,
                         uint32_t interval_ms = 30000)
    {
        callback = cb;
        timeout = timeout_ms;
        interval = interval_ms;
    }
    ~WatchdogCfg() {}

    uint32_t timeout;
    uint32_t interval;
    ffrt_watchdog_cb *callback;

    static inline WatchdogCfg* Instance(ffrt_watchdog_cb* cb = nullptr, uint32_t timeout_ms = 30000,
                                        uint32_t interval_ms = 30000)
    {
        static WatchdogCfg inst(cb, timeout_ms, interval_ms);
        return &inst;
    }
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
void ffrt_watchdog_register(ffrt_watchdog_cb *cb, uint32_t timeout_ms, uint32_t interval_ms)
{
    ffrt::WatchdogCfg::Instance(cb, timeout_ms, interval_ms);
}

API_ATTRIBUTE((visibility("default")))
ffrt_watchdog_cb *ffrt_watchdog_get_cb(void)
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
