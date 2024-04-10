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

#ifdef OHOS_STANDARD_SYSTEM
#include "faultloggerd_client.h"
#endif
#include <string>
#include <atomic>
#include "ffrt_log_api.h"
#include "internal_inc/osal.h"

static int g_ffrtLogLevel = FFRT_LOG_DEBUG;
static std::atomic<unsigned int> g_ffrtLogId(0);

unsigned int GetLogId(void)
{
    return ++g_ffrtLogId;
}

int GetFFRTLogLevel(void)
{
    return g_ffrtLogLevel;
}

static void SetLogLevel(void)
{
    std::string envLogStr = GetEnv("FFRT_LOG_LEVEL");
    if (envLogStr.size() != 0) {
        int level = std::stoi(envLogStr);
        if (level < FFRT_LOG_LEVEL_MAX && level >= FFRT_LOG_ERROR) {
            g_ffrtLogLevel = level;
            return;
        }
    }
}

static __attribute__((constructor)) void LogInit(void)
{
    SetLogLevel();
}