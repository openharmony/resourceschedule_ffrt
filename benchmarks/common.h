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

#ifndef BENCHMARKS_COMMON
#define BENCHMARKS_COMMON

#include <chrono>
#include <cstdlib>
#include <inttypes.h>
#include "ffrt_inner.h"

size_t COMPUTE_TIME_US = 0;
uint64_t REPEAT = 1;
uint64_t PREHOT_FFRT = 1;
uint64_t FIB_NUM = 0;

#define CLOCK std::chrono::steady_clock::now()
#define TIME_BEGIN(t) auto __##t##_start = CLOCK
#define TIME_END(t) \
    do { \
        decltype(__##t##_start) __##t##_cur = CLOCK; \
        ("%-12s:%-4d %6lu us\n", __FILE__, __LINE__, \
            long(std::chrono::duration_cast<std::chrono::microseconds>(__##t##_cur - __##t##_start).count())); \
    } while (0)
#define TIME_END_INFO(t, info) \
    do { \
        decltype(__##t##_start) __##t##_cur = CLOCK; \
        printf("%-12s:%-4d %s %6lu us\n", __FILE__, __LINE__, info, \
            long(std::chrono::duration_cast<std::chrono::microseconds>(__##t##_cur - __##t##_start).count())); \
    } while (0)

#define GET_ENV(name, var, default) \
    do { \
        auto __get_env_##name = getenv(#name); \
        var = __get_env_##name ? atoi(__get_env_##name) : default; \
        printf(#name " = %" PRIu64 "\n", (uint64_t)(var)); \
    } while (0)

#define EXPECT(cond) \
    if (!(cond)) { \
        printf(#cond " check failed\n"); \
    }

static inline void simulate_task_compute_time(size_t us)
{
    auto start = std::chrono::steady_clock::now();
    size_t passed = 0;
    while (passed < us) {
        passed =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    }
}

static void FibFFRT(int x, int& y)
{
    if (x <= 1) {
        y = x;
    } else {
        int y1;
        int y2;
        ffrt::submit([&]() { FibFFRT(x - 1, y1); }, {&x}, {&y1});
        ffrt::submit([&]() { FibFFRT(x - 2, y2); }, {&x}, {&y2});
        ffrt::wait({&y1, &y2});
        y = y1 + y2;
    }
    simulate_task_compute_time(COMPUTE_TIME_US);
}

void PreHotFFRT(void)
{
    if (!PREHOT_FFRT) {
        return;
    }
    int output;
    ffrt::submit([&]() { FibFFRT(20, output); }, {}, {&output});
    ffrt::wait({&output});
}

void GetEnvs(void)
{
    GET_ENV(COMPUTE_TIME_US, COMPUTE_TIME_US, 0);
    GET_ENV(REPEAT, REPEAT, 1);
    GET_ENV(PREHOT_FFRT, PREHOT_FFRT, 0);
    GET_ENV(FIB_NUM, FIB_NUM, 5);
}

static inline void completely_paralle(uint32_t count, uint32_t duration, int64_t& time)
{
    uint32_t loop = count;
    auto start = std::chrono::steady_clock::now();
    while (loop--) {
        ffrt::submit([&]() { simulate_task_compute_time(duration); }, {}, {});
    }
    ffrt::wait();
    time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}

static inline void completely_serial(uint32_t count, uint32_t duration, int64_t& time)
{
    uint32_t x = 0;
    uint32_t loop = count;
    auto start = std::chrono::steady_clock::now();
    while (loop--) {
        ffrt::submit([&]() { simulate_task_compute_time(duration); }, {&x}, {&x});
    }
    ffrt::wait();
    time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}
static inline void single_thread(uint32_t count, uint32_t duration, int64_t& time)
{
    uint32_t loop = count;
    auto start = std::chrono::steady_clock::now();
    while (loop--) {
        simulate_task_compute_time(duration);
    }
    time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
}
#endif