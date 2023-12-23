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

#include "ffrt_inner.h"
#include "common.h"

using namespace ffrt;
void Fib(int x, int& y)
{
    if (x <= 1) {
        y = x;
    } else {
        int y1;
        int y2;
        Fib(x - 1, y1);
        Fib(x - 2, y2);
        y = y1 + y2;
    }
    simulate_task_compute_time(COMPUTE_TIME_US);
}

void FibFFRTChildWait(int x, int& y)
{
    if (x <= 1) {
        y = x;
    } else {
        int y1;
        int y2;
        ffrt::submit([&]() { FibFFRTChildWait(x - 1, y1); }, {}, {});
        ffrt::submit([&]() { FibFFRTChildWait(x - 2, y2); }, {}, {});
        ffrt::wait();
        y = y1 + y2;
    }
    simulate_task_compute_time(COMPUTE_TIME_US);
}

void FibFFRTDataWait(int x, int& y)
{
    if (x <= 1) {
        y = x;
    } else {
        int y1;
        int y2;
        ffrt::submit([&]() { FibFFRTDataWait(x - 1, y1); }, {}, {&y1});
        ffrt::submit([&]() { FibFFRTDataWait(x - 2, y2); }, {}, {&y2});
        ffrt::wait({&y1, &y2});
        y = y1 + y2;
    }
    simulate_task_compute_time(COMPUTE_TIME_US);
}

void FibDataWait()
{
    PreHotFFRT();

    int output;

    {
        int expect;
        Fib(FIB_NUM, expect);
    }

    TIME_BEGIN(t);
    for (uint64_t i = 0; i < REPEAT; ++i) {
        ffrt::submit([&]() { FibFFRTDataWait(FIB_NUM, output); }, {}, {&output});
        ffrt::wait({&output});
    }
    TIME_END_INFO(t, "fib_data_wait");
}

void FibFFRTNoWait(int x, int* y)
{
    if (x <= 1) {
        *y = x;
    } else {
        int *y1, *y2;
        y1 = reinterpret_cast<int *>(malloc(sizeof(int)));
        y2 = reinterpret_cast<int *>(malloc(sizeof(int)));
        ffrt::submit([=]() { FibFFRTNoWait(x - 1, y1); }, {}, {y1});
        ffrt::submit([=]() { FibFFRTNoWait(x - 2, y2); }, {}, {y2});
        ffrt::submit(
            [=]() {
                *y = *y1 + *y2;
                free(y1);
                free(y2);
            },
            {y1, y2}, {y});
    }
    simulate_task_compute_time(COMPUTE_TIME_US);
}

void FibNoWait()
{
    PreHotFFRT();

    int output;

    {
        int expect;
        Fib(FIB_NUM, expect);
    }

    TIME_BEGIN(t);
    for (uint64_t i = 0; i < REPEAT; ++i) {
        ffrt::submit([&]() { FibFFRTNoWait(FIB_NUM, &output); }, {}, {&output});
        ffrt::wait({&output});
    }
    TIME_END_INFO(t, "fib_no_wait");
}

void FibChildWait()
{
    PreHotFFRT();

    int output;
    {
        int expect;
        Fib(FIB_NUM, expect);
    }

    TIME_BEGIN(t);
    for (uint64_t i = 0; i < REPEAT; ++i) {
        ffrt::submit([&]() { FibFFRTChildWait(FIB_NUM, output); }, {}, {&output});
        ffrt::wait({&output});
    }
    TIME_END_INFO(t, "fib_child_wait");
}

int main()
{
    GetEnvs();
    FibDataWait();
    FibChildWait();
    FibNoWait();
}