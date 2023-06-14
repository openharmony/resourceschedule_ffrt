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

#include "ffrt.h"
#include "common.h"

constexpr uint32_t SLICE_NUM = 2400;
constexpr uint32_t BUFFER_NUM = 2;

auto gpuPreTask = []() { simulate_task_compute_time(COMPUTE_TIME_US); };
// NPU task，使用锁来模拟只有一个NPU硬件，对ffrt来说，该任务是阻塞型任务
auto npuTask = []() { simulate_task_compute_time(COMPUTE_TIME_US); };
auto gpuPostTask = []() { simulate_task_compute_time(COMPUTE_TIME_US); };

void AIRaw()
{
    PreHotFFRT();

    int pre_outbuf[BUFFER_NUM] = { 0 };
    int npu_outbuf[BUFFER_NUM] = { 0 };

    TIME_BEGIN(t);
    for (uint32_t r = 0; r < REPEAT; r++) {
        for (uint32_t i = 0; i < SLICE_NUM; i++) {
            uint32_t buf_id = i % BUFFER_NUM;
            ffrt::submit(gpuPreTask, {}, {pre_outbuf + buf_id});
            ffrt::submit(npuTask, {pre_outbuf + buf_id}, {npu_outbuf + buf_id});
            ffrt::submit(gpuPostTask, {npu_outbuf + buf_id}, {});
        }
        ffrt::wait();
    }
    TIME_END_INFO(t, "airaw");
}

void AIRawWorker()
{
    PreHotFFRT();

    int pre_outbuf[BUFFER_NUM] = { 0 };
    int npu_outbuf[BUFFER_NUM] = { 0 };

    TIME_BEGIN(t);
    for (uint32_t r = 0; r < REPEAT; r++) {
        ffrt::submit(
            [&]() {
                for (uint32_t i = 0; i < SLICE_NUM; i++) {
                    uint32_t buf_id = i % BUFFER_NUM;
                    ffrt::submit(gpuPreTask, {}, {pre_outbuf + buf_id});
                    ffrt::submit(npuTask, {pre_outbuf + buf_id}, {npu_outbuf + buf_id});
                    ffrt::submit(gpuPostTask, {npu_outbuf + buf_id}, {});
                }
                ffrt::wait();
            },
            {}, {&r});
        ffrt::wait({&r});
    }
    TIME_END_INFO(t, "airaw_worker_submit");
}

int main()
{
    GetEnvs();
    AIRaw();
    AIRawWorker();
}