/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef FFRT_WHITE_LIST
#define FFRT_WHITE_LIST
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <array>

namespace ffrt {

enum class WhiteListKey : uint8_t {
    log_ctr,
    worker_monitor,
    SetThreadAttr,
    IsDelayedWorkerPreserved,
    IsInSFFRTListOHOS,
    IsInSFFRTList,
    WhiteListTest,
    PerfTraceScopedAll,
    PerfTraceScopedCustom,
    PerfTraceScopedEu,
    PerfTraceScopedSched,
    PerfTraceScopedDm,
    PerfTraceScopedQueue,
    PerfTraceScopedSync,
    DisableLIFOFutex,
    SetThreadInitPri,
    EnableSchedQss,
    BlackListQss,
    UNKNOWN,
    KEY_MAX = UNKNOWN
};

class WhiteList {
public:
    static WhiteList& GetInstance();
    void LoadFromFile();
    bool IsEnabled(WhiteListKey key, bool defaultWhenAbnormal);

private:
    WhiteList();
    bool TryRefreshWhiteListOnInit();

    WhiteList(const WhiteList&) = delete;
    WhiteList& operator=(const WhiteList&) = delete;

    static constexpr size_t KEY_COUNT = static_cast<size_t>(WhiteListKey::KEY_MAX);
    std::array<int, KEY_COUNT> whiteListArray_{};
    std::mutex whitelistMutex_;
    bool whitelistInited_ = false;
};
} // namespace ffrt
#endif