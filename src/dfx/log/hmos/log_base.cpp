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

#include "dfx/log/log_base.h"
#include <securec.h>
#include <string>
#include "hilog/log.h"

static constexpr unsigned int FFRT_LOG_DOMAIN = 0xD001719;
static constexpr char FFRT_LOG_TAG[] = "ffrt";

inline void StringReplace(std::string& str, const std::string& oldStr, const std::string& newStr)
{
    std::string::size_type pos = 0u;
    while ((pos = str.find(oldStr, pos)) != std::string::npos) {
        str.replace(pos, oldStr.length(), newStr);
        pos += newStr.length();
    }
}
static inline void PrintLogString(LogLevel logLevel, const char* fmt, va_list arg)
{
    char buf[1024] = {0};
    std::string format(fmt);
    StringReplace(format, "%{public}", "%");
    if (vsnprintf_s(buf, sizeof(buf), sizeof(buf) - 1, format.c_str(), arg) > 0) {
        HiLogPrint(LOG_CORE, logLevel, FFRT_LOG_DOMAIN, FFRT_LOG_TAG, "%{public}s", buf);
    }
}

void LogErr(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    PrintLogString(LOG_ERROR, format, args);
    va_end(args);
}

void LogWarn(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    PrintLogString(LOG_WARN, format, args);
    va_end(args);
}

void LogInfo(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    PrintLogString(LOG_INFO, format, args);
    va_end(args);
}

void LogDebug(const char* format, ...)
{
    if (HiLogIsLoggable(FFRT_LOG_DOMAIN, "ffrt", LOG_INFO)) {
        va_list args;
        va_start(args, format);
        PrintLogString(LOG_DEBUG, format, args);
        va_end(args);
    } else {
        SetFFRTLogLevel(FFRT_LOG_WARN);
    }
}