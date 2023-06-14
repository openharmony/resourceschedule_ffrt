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

#include "mutex_perf.h"

namespace ffrt {
MutexStatistic::~MutexStatistic()
{
    printf("***Mutex Cycles Statistic***\n");
    for (auto& it : cycles_) {
        printf("***%s: %u***\n", it.first.c_str(), it.second);
    }
}

static MutexStatistic gMutexStatistic;

void AddMutexCycles(std::string key, uint32_t val)
{
    std::lock_guard<decltype(gMutexStatistic.mtx_)> lg(gMutexStatistic.mtx_);
    if (gMutexStatistic.cycles_.find(key) != gMutexStatistic.cycles_.end()) {
        gMutexStatistic.cycles_[key] += val;
    } else {
        gMutexStatistic.cycles_[key] = val;
    }
}
} // namespace ffrt