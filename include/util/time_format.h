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
#include <unordered_map>
#include "internal_inc/types.h"

namespace ffrt {
typedef enum {
    MILLISECOND,
    MICROSECOND,
} TimeUnitT;

std::string FormatDateString4SystemClock(const std::chrono::system_clock::time_point& timePoint,
    TimeUnitT timeUnit = MILLISECOND);
std::string FormatDateString4SteadyClock(uint64_t steadyClockTimeStamp, TimeUnitT timeUnit = MILLISECOND);
std::string FormatDateString4CntCt(uint64_t cntCtTimeStamp, TimeUnitT timeUnit = MILLISECOND);
std::string FormatDateToString(uint64_t timeStamp);
uint64_t Arm64CntFrq(void);
uint64_t Arm64CntCt(void);
uint64_t ConvertCntvctToUs(uint64_t cntCt);
uint64_t ConvertUsToCntvct(uint64_t time);
uint64_t ConvertTscToSteadyClockCount(uint64_t cntCt);

inline uint64_t TimeStampCntvct(void)
{
#if defined(__aarch64__)
    // 在arm环境下获取当前的CPU cycle数的时间戳，单位纳秒颗粒度
    return Arm64CntCt();
#else
    // 在非arm环境下获取std的时间戳，单位微秒
    return static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()).time_since_epoch().count());
#endif
}

inline uint64_t TimeStampSteady(void)
{
    return static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()).time_since_epoch().count());
}
}
#endif // UTIL_TIME_FORAMT_H
