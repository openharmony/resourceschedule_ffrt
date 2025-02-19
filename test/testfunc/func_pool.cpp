/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <vector>
#include <cmath>
#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include "ffrt_inner.h"
#ifdef USE_GTEST
#include <gtest/gtest.h>
#else
#include "ptest.h"
#endif
#include "dfx/log/ffrt_log_api.h"

using namespace std;
using namespace ffrt;

// its value is equles to the number of submitted task
void NestedFib(int num, int &count)
{
    if (num <= 1) {
        count = 0;
    } else {
        int val1;
        int val2;
        ffrt::submit([&]() { NestedFib(num - 1, val1); }, {}, { &val1 });
        ffrt::submit([&]() { NestedFib(num - 2, val2); }, {}, { &val2 });
        ffrt::wait({ &val1, &val2 });
        count = val1 + val2 + 2;
    }
}

void NestedAddOne(int deepth, int &val)
{
    if (deepth == 0) {
        val = 0;
    } else {
        ffrt::submit([&]() { NestedAddOne(deepth - 1, val); }, { &val }, { &val },
            ffrt::task_attr().name(("n" + std::to_string(deepth)).c_str()));
        ffrt::wait({ &val });
        val += 1;
    }
}

void NestedWhile(uint64_t count)
{
    int x = 1;
    int y0;
    int y1;
    int y2;
    int y3;
    int y4;
    int y5;
    int y6;
    int y7;
    int y8;
    int y9;
    int i = 1;
    while (count--) {
        ffrt::submit(
            [&]() {
                ffrt::submit(
                    [&]() {
                        ffrt::submit(
                            [&]() {
                                ffrt::submit(
                                    [&]() {
                                        ffrt::submit(
                                            [&]() {
                                                ffrt::submit(
                                                    [&]() {
                                                        ffrt::submit(
                                                            [&]() {
                                                                ffrt::submit(
                                                                    [&]() {
                                                                        ffrt::submit(
                                                                            [&]() {
                                                                                ffrt::submit(
                                                                                    [&]() {
                                                                                        ffrt::submit(
                                                                                            [&]() {
                                                                                                y9 = x + 1;
                                                                                                EXPECT_EQ(y9, 2);
                                                                                            },
                                                                                            { &x }, { &y9 },
                                                                                            ffrt::task_attr()
                                                                                            .name(("y9s1w" +
                                                                                            to_string(i))
                                                                                            .c_str()));
                                                                                        ffrt::submit(
                                                                                            [&]() {
                                                                                                y9++;
                                                                                                EXPECT_EQ(y9, 3);
                                                                                            },
                                                                                            { &y9 }, { &y9 },
                                                                                            ffrt::task_attr()
                                                                                            .name(("y9s2w" +
                                                                                            to_string(i))
                                                                                            .c_str()));
                                                                                        ffrt::wait();
                                                                                    },
                                                                                    {}, { &y9 },
                                                                                    ffrt::task_attr()
                                                                                    .name(("y9w" + to_string(i))
                                                                                    .c_str()));
                                                                                ffrt::submit(
                                                                                    [&]() {
                                                                                        y8 = y9 + 1;
                                                                                        EXPECT_EQ(y8, 4);
                                                                                    },
                                                                                    { &y9 }, { &y8 },
                                                                                    ffrt::task_attr()
                                                                                    .name(("y8s1w" + to_string(i))
                                                                                    .c_str()));
                                                                                ffrt::wait();
                                                                            },
                                                                            {}, { &y8 },
                                                                            ffrt::task_attr()
                                                                            .name(("y8w" + to_string(i))
                                                                            .c_str()));
                                                                        ffrt::submit(
                                                                            [&]() {
                                                                                y7 = y8 + 1;
                                                                                EXPECT_EQ(y7, 5);
                                                                            },
                                                                            { &y8 }, { &y7 },
                                                                            ffrt::task_attr()
                                                                            .name(("y7s1w" + to_string(i))
                                                                            .c_str()));
                                                                        ffrt::wait();
                                                                    },
                                                                    {}, { &y7 },
                                                                    ffrt::task_attr()
                                                                    .name(("y7w" + to_string(i))
                                                                    .c_str()));
                                                                ffrt::submit(
                                                                    [&]() {
                                                                        y6 = y7 + 1;
                                                                        EXPECT_EQ(y6, 6);
                                                                    },
                                                                    { &y7 }, { &y6 },
                                                                    ffrt::task_attr()
                                                                    .name(("y6s1w" + to_string(i))
                                                                    .c_str()));
                                                                ffrt::wait();
                                                            },
                                                            {}, { &y6 },
                                                            ffrt::task_attr().name(("y6w" + to_string(i)).c_str()));
                                                        ffrt::submit(
                                                            [&]() {
                                                                y5 = y6 + 1;
                                                                EXPECT_EQ(y5, 7);
                                                            },
                                                            { &y6 }, { &y5 },
                                                            ffrt::task_attr()
                                                            .name(("y5s1w" + to_string(i))
                                                            .c_str()));
                                                        ffrt::wait();
                                                    },
                                                    {}, { &y5 },
                                                    ffrt::task_attr().name(("y5w" + to_string(i)).c_str()));
                                                ffrt::submit(
                                                    [&]() {
                                                        y4 = y5 + 1;
                                                        EXPECT_EQ(y4, 8);
                                                    },
                                                    { &y5 }, { &y4 },
                                                    ffrt::task_attr().name(("y4s1w" + to_string(i)).c_str()));
                                                ffrt::wait();
                                            },
                                            {}, { &y4 }, ffrt::task_attr().name(("y4w" + to_string(i)).c_str()));
                                        ffrt::submit(
                                            [&]() {
                                                y3 = y4 + 1;
                                                EXPECT_EQ(y3, 9);
                                            },
                                            { &y4 }, { &y3 }, ffrt::task_attr().name(("y3s1w" + to_string(i))
                                            .c_str()));
                                        ffrt::wait();
                                    },
                                    {}, { &y3 }, ffrt::task_attr().name(("y3w" + to_string(i)).c_str()));
                                ffrt::submit(
                                    [&]() {
                                        y2 = y3 + 1;
                                        EXPECT_EQ(y2, 10);
                                    },
                                    { &y3 }, { &y2 }, ffrt::task_attr().name(("y2s1w" + to_string(i)).c_str()));
                                ffrt::wait();
                            },
                            {}, { &y2 }, ffrt::task_attr().name(("y2w" + to_string(i)).c_str()));
                        ffrt::submit(
                            [&]() {
                                y1 = y2 + 1;
                                EXPECT_EQ(y1, 11);
                            },
                            { &y2 }, { &y1 }, ffrt::task_attr().name(("y1s1w" + to_string(i)).c_str()));
                        ffrt::wait();
                    },
                    {}, { &y1 }, ffrt::task_attr().name(("y1w" + to_string(i)).c_str()));
                ffrt::submit(
                    [&]() {
                        y0 = y1 + 1;
                        EXPECT_EQ(y0, 12);
                    },
                    { &y1 }, { &y0 }, ffrt::task_attr().name(("y0s1w" + to_string(i)).c_str()));
                ffrt::wait();
            },
            {}, { &y0 }, ffrt::task_attr().name(("y0w" + to_string(i)).c_str()));
        ffrt::wait({ &y0 });
        EXPECT_EQ(y0, 12);
        i++;
    }
}
