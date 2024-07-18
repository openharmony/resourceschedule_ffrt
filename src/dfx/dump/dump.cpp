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
#include <securec.h>
#include "c/ffrt_dump.h"
#include "dfx/bbox/bbox.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
constexpr uint32_t DEFAULT_TIMEOUT_MS = 30000;
struct TimeoutCfg {
    static inline TimeoutCfg* Instance()
    {
        static TimeoutCfg inst;
        return &inst;
    }

    uint32_t timeout = DEFAULT_TIMEOUT_MS;
    ffrt_task_timeout_cb callback = nullptr;
};
}

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

API_ATTRIBUTE((visibility("default")))
int dump_info_all(char *buf, uint32_t len)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    if (FFRTIsWork()) {
        std::string dumpInfo;
        dumpInfo += "|-> Launcher proc ffrt, pid:" + std::to_string(GetPid()) + "\n";
        dumpInfo += SaveTaskCounterInfo();
        dumpInfo += SaveWorkerStatusInfo();
        dumpInfo += SaveReadyQueueStatusInfo();
        dumpInfo += SaveNormalTaskStatusInfo();
        dumpInfo += SaveQueueTaskStatusInfo();
        if (dumpInfo.length() > (len - 1)) {
            FFRT_LOGW("dumpInfo exceeds the buffer length, length:%d", dumpInfo.length());
        }
        return snprintf_s(buf, len, len - 1, "%s", dumpInfo.c_str());
    } else {
        return snprintf_s(buf, len, len - 1, "|-> FFRT has done all tasks, pid: %u \n",GetPid());
    }
#else
    return -1;
#endif
}

API_ATTRIBUTE((visibility("default")))
int ffrt_dump(ffrt_dump_cmd_t cmd, char *buf, uint32_t len)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    switch (cmd) {
        case ffrt_dump_cmd_t::DUMP_INFO_ALL: {
            return dump_info_all(buf, len);
        }
        default: {
            FFRT_LOGE("ffr_dump unsupport cmd[%d]", cmd);
        }
    }
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
    return -1;
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_timeout_cb ffrt_task_timeout_get_cb(void)
{
    return ffrt::TimeoutCfg::Instance()->callback;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_timeout_set_cb(ffrt_task_timeout_cb cb)
{
    ffrt::TimeoutCfg::Instance()->callback = cb;
}

API_ATTRIBUTE((visibility("default")))
uint32_t ffrt_task_timeout_get_threshold(void)
{
    return ffrt::TimeoutCfg::Instance()->timeout;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_timeout_set_threshold(uint32_t threshold_ms)
{
    ffrt::TimeoutCfg::Instance()->timeout = threshold_ms;
}
#ifdef __cplusplus
}
#endif
