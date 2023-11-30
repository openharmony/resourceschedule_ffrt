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

#include <random>

#include "ffrt_inner.h"
#include "common.h"

static constexpr uint64_t sz = 30; // 该值越大任务平均可并行度越大（平均并发度=sz/9）
static constexpr uint64_t iter = 1000000; // 该值越大迭代次数越多（减少测量误差）
static constexpr uint64_t depth = 10; // 该值越大子任务平均粒度越大（任务完成时间为泊松分布）（单位：微秒）

static inline uint64_t func(uint64_t x, uint64_t y)
{
    std::mt19937_64 g(x - y);
    uint64_t target = g() % (depth * 20);
    uint64_t acc = 0;
    while (acc % (depth * 20) != target) {
        acc ^= g();
    }
    return acc;
}

static inline void GenerateIndexes(std::mt19937_64& rnd, uint64_t(idx)[3])
{
    bool duplicate = true;
    while (duplicate) {
        duplicate = false;
        for (uint64_t i = 0; i < 3; i++) {
            idx[i] = rnd() % sz;
            for (uint64_t z = 0; z < i; z++) {
                if (idx[z] == idx[i]) {
                    duplicate = true;
                }
            }
        }
    }
}

static uint64_t BenchmarkNative()
{
    uint64_t* arr = new uint64_t[sz];
    std::mt19937_64 rnd(0);
    // initialize the array
    for (uint64_t i = 0; i < sz; i++) {
        arr[i] = rnd();
    }

    // do computation randomly
    TIME_BEGIN(t);
    for (uint64_t i = 0; i < iter; i++) {
        // generate 3 different indexes
        uint64_t idx[3] = {};
        GenerateIndexes(rnd, idx);

        // submit a task
        arr[idx[2]] = func(arr[idx[0]], arr[idx[1]]);
    }
    TIME_END_INFO(t, "benchmark_native");

    // calculate FNV hash of the array
    uint64_t hash = 14695981039346656037ULL;
    for (uint64_t i = 0; i < sz; i++) {
        hash = (hash * 1099511628211ULL) ^ arr[i];
    }
    delete[] arr;
    return hash;
}

static uint64_t BenchmarkFFRT()
{
    uint64_t* arr = new uint64_t[sz];
    std::mt19937_64 rnd(0);
    // initialize the array
    for (uint64_t i = 0; i < sz; i++) {
        arr[i] = rnd();
    }
    // do computation randomly
    TIME_BEGIN(t);
    for (uint64_t i = 0; i < iter; i++) {
        // generate 3 different indexes
        uint64_t idx[3] = {};
        GenerateIndexes(rnd, idx);

        // submit a task
        ffrt::submit([idx, &arr]() { arr[idx[2]] = func(arr[idx[0]], arr[idx[1]]); }, {&arr[idx[0]], &arr[idx[1]]},
            {&arr[idx[2]]});
    }
    ffrt::wait();
    TIME_END_INFO(t, "benchmark_ffrt");

    // calculate FNV hash of the array
    uint64_t hash = 14695981039346656037ULL;
    for (uint64_t i = 0; i < sz; i++) {
        hash = (hash * 1099511628211ULL) ^ arr[i];
    }
    delete[] arr;
    return hash;
}

int main()
{
    GetEnvs();
    BenchmarkNative();
    BenchmarkFFRT();
}