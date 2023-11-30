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

int main()
{
    int64_t ffrt_time = 0;
    uint32_t count = 10000;
    uint32_t max_min = 2000; // us
    uint32_t min = 0;
    uint32_t max = max_min;
    completely_paralle(count, max_min, ffrt_time); // pre hot ffrt
    while ((max - min) > 1) {
        // use binay search to get the min granularity of tasks that can obtain benefits
        uint32_t median = (max + min) / 2;
        completely_paralle(count, median, ffrt_time);
        float speedup = (1.0f * count * median) / ffrt_time;
        printf("duration:%u speedup:%.2f\n", median, speedup);
        if (speedup > 1) {
            max = median;
        } else {
            min = median;
        }
    }
    return 0;
}

