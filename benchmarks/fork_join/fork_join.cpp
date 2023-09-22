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

constexpr uint32_t FORK_JOIN_COUNT = 10000;

void ForkJoin()
{
    PreHotFFRT();

    TIME_BEGIN(t);
    for (uint32_t r = 0; r < REPEAT; r++) {
        for (uint32_t i = 0; i < FORK_JOIN_COUNT; i++) {
            ffrt::submit([=]() { simulate_task_compute_time(COMPUTE_TIME_US); }, {}, {});
        }
        ffrt::wait();
    }
    TIME_END_INFO(t, "fork_join");
}

void ForkJoinWorker()
{
    PreHotFFRT();

    TIME_BEGIN(t);
    for (uint32_t r = 0; r < REPEAT; r++) {
        ffrt::submit(
            [&]() {
                for (uint32_t i = 0; i < FORK_JOIN_COUNT; i++) {
                    ffrt::submit([=]() { simulate_task_compute_time(COMPUTE_TIME_US); }, {}, {});
                }
                ffrt::wait();
            },
            {}, {&r});
        ffrt::wait({&r});
    }
    TIME_END_INFO(t, "fork_join_worker_submit");
}

int main()
{
    GetEnvs();
    ForkJoin();
    ForkJoinWorker();
}