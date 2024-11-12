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
#ifndef UTIL_TIME_FORMAT_H
#define UTIL_TIME_FORMAT_H

#include <chrono>
#include <string>
#include <securec.h>

namespace ffrt {
typedef enum {
    millisecond,
    microsecond,
} time_unit_t;

static std::string FormatDateString(const std::chrono::system_clock::time_point& timePoint,
    time_unit_t timeUnit = millisecond)
{
    constexpr int MaxMsLength = 3;
    constexpr int MsPerSecond = 1000;
    constexpr int DatetimeStringLength = 80;
    constexpr int MaxUsLength = 6;
    constexpr int UsPerSecond = 1000 * 1000;

    std::string remainder;
    if (timeUnit == microsecond) {
        auto tp = std::chrono::time_point_cast<std::chrono::microseconds>(timePoint);
        auto us = tp.time_since_epoch().count() % UsPerSecond;
        remainder = std::to_string(us);
        if (remainder.length() < MaxUsLength) {
            remainder = std::string(MaxUsLength - remainder.length(), '0') + remainder;
        }
    } else {
        auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(timePoint);
        auto ms = tp.time_since_epoch().count() % MsPerSecond;
        remainder = std::to_string(ms);
        if (remainder.length() < MaxMsLength) {
            remainder = std::string(MaxMsLength - remainder.length(), '0') + remainder;
        }
    }
    auto tt = std::chrono::system_clock::to_time_t(timePoint);
    struct tm curTime;
    if (memset_s(&curTime, sizeof(curTime), 0, sizeof(curTime)) != EOK) {
        FFRT_LOGE("Fail to memset");
        return "";
    }
    localtime_r(&tt, &curTime);
    char sysTime[DatetimeStringLength];
    std::strftime(sysTime, sizeof(char) * DatetimeStringLength, "%Y-%m-%d %H:%M:%S.", &curTime);
    return std::string(sysTime) + remainder;
}

static std::string FormatDateString4SteadyClock(uint64_t steadyClockTimeStamp, time_unit_t timeUnit = millisecond)
{
    static uint64_t globalTimeStamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    static auto globalTp = std::chrono::system_clock::now();

    std::chrono::microseconds us((int64_t)(steadyClockTimeStamp - globalTimeStamp));
    return FormatDateString(globalTp + us, timeUnit);
}

static inline uint64_t Arm64CntFrq(void)
{
    uint64_t freq = 1;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (freq));
    return freq;
}

static inline uint64_t Arm64CntCt(void)
{
    uint64_t tsc = 1;
    asm volatile("mrs %0, cntvct_el0" : "=r" (tsc));
    return tsc;
}

static std::string FormatDateString4CntCt(uint64_t cntCtTimeStamp, time_unit_t timeUnit = millisecond)
{
    constexpr int Ratio = 1000 * 1000;

    static int64_t globalFreq = Arm64CntFrq();
    if (globalFreq == 0) {
        return "";
    }
    static uint64_t globalCntCt = Arm64CntCt();
    static auto globalTp = std::chrono::system_clock::now();
    std::chrono::microseconds us((int64_t)(cntCtTimeStamp - globalCntCt) * Ratio / globalFreq);
    return FormatDateString(globalTp + us, timeUnit);
}
}
#endif // UTIL_TIME_FORAMT_H
