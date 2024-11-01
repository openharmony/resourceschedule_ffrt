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
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <atomic>
#include "ffrt_log_api.h"
#include "internal_inc/osal.h"
static int g_ffrtLogLevel = FFRT_LOG_DEBUG;
static std::atomic<unsigned int> g_ffrtLogId(0);
static bool g_whiteListFlag = false;
namespace {
    constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
    constexpr char CONF_FILEPATH[] = "/etc/ffrt/log_ctr_whitelist.conf";
}

unsigned int GetLogId(void)
{
    return ++g_ffrtLogId;
}

bool IsInWhitelist(void)
{
    return g_whiteListFlag;
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

void InitWhiteListFlag(void)
{
    // 获取当前进程名称
    char processName[PROCESS_NAME_BUFFER_LENGTH] = "";
    GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
    if (strlen(processName) == 0) {
        g_whiteListFlag = true;
        return;
    }

    // 从配置文件读取白名单对比
    std::string whiteProcess;
    std::ifstream file(CONF_FILEPATH);
    if (file.is_open()) {
        while (std::getline(file, whiteProcess)) {
            if (strstr(processName, whiteProcess.c_str()) != nullptr) {
                g_whiteListFlag = true;
                return;
            }
        }
    } else {
        // 当文件不存在或者无权限时默认都开
        g_whiteListFlag = true;
    }
}

static __attribute__((constructor)) void LogInit(void)
{
    SetLogLevel();
    InitWhiteListFlag();
}