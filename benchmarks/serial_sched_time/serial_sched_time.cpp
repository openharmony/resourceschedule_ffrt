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

static std::vector<uint32_t> duration_sample = {50, 60, 70, 80, 90, 100, 120, 140, 160, 180, 200, 500, 1000};

int main()
{
    int64_t ffrt_time;
    int64_t single_t_time;
    uint32_t count = 10000;
    PreHotFFRT();

    for (uint32_t i = 0; i < duration_sample.size(); i++) {
        single_thread(count, duration_sample[i], single_t_time);
        completely_serial(count, duration_sample[i], ffrt_time);
        float sched_time = (1.0f * ffrt_time - single_t_time) / count;
        printf("completely serial count:%u duration:%u ffrt_time:%ld single_t_time:%ld "
               "sched_time:%.2f\n",
            count, duration_sample[i], ffrt_time, single_t_time, sched_time);
    }
    return 0;
}
