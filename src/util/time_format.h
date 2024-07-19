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
#ifndef UTIL_TIME_FORAMT_H
#define UTIL_TIME_FORAMT_H

#include <chrono>
#include <string>
#include <securec.h>

namespace ffrt {
std::string FormatDateString(const std::chrono::system_clock::time_point& timePoint)
{
    constexpr int MaxMsLength = 3;
    constexpr int MsPerSecond = 1000;
    constexpr int DatetimeStringLength = 80;

    auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(timePoint);
    auto tt = std::chrono::system_clock::to_time_t(timePoint);
    auto ms = tp.time_since_epoch().count() % MsPerSecond;
    auto msString = std::to_string(ms);
    if (msString.length() < MaxMsLength) {
        msString = std::string(MaxMsLength - msString.length(), '0') + msString;
    }
    struct tm curTime;
    if (memset_s(&curTime, sizeof(curTime), 0, sizeof(curTime)) != EOK) {
        FFRT_LOGE("Fail to memset");
        return "";
    }
    localtime_r(&tt, &curTime);
    char sysTime[DatetimeStringLength];
    std::strftime(sysTime, sizeof(char) * DatetimeStringLength, "%Y-%m-%d %I:%M:%S.", &curTime);
    return std::string(sysTime) + msString;
}

std::string FormatDateString(uint64_t steadyClockTimeStamp)
{
    std::chrono::microseconds ms(steadyClockTimeStamp - std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    auto tp = std::chrono::system_clock::now() + ms;
    return FormatDateString(tp);
}
}

#endif // UTIL_TIME_FORAMT_H
