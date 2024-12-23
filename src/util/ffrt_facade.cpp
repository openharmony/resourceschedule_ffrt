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
#include "util/ffrt_facade.h"
#include "dfx/log/ffrt_log_api.h"

namespace {
std::atomic<bool> g_exitFlag { false };
std::shared_mutex g_exitMtx;
std::atomic<bool> g_delayedWorkerExitFlag { false };
}

namespace ffrt {
bool GetExitFlag()
{
    return g_exitFlag.load();
}

std::shared_mutex& GetExitMtx()
{
    return g_exitMtx;
}

bool GetDelayedWorkerExitFlag()
{
    return g_delayedWorkerExitFlag.load();
}

void SetDelayedWorkerExitFlag()
{
    g_delayedWorkerExitFlag.store(true);
}

class ProcessExitManager {
public:
    static ProcessExitManager& Instance()
    {
        static ProcessExitManager instance;
        return instance;
    }

    ProcessExitManager(const ProcessExitManager&) = delete;
    ProcessExitManager& operator=(const ProcessExitManager&) = delete;

private:
    ProcessExitManager() {}

    ~ProcessExitManager()
    {
        FFRT_LOGW("ProcessExitManager destruction enter");
        std::unique_lock lock(g_exitMtx);
        g_exitFlag.store(true);
    }
};

FFRTFacade::FFRTFacade()
{
    DependenceManager::Instance();
    ProcessExitManager::Instance();
    InitWhiteListFlag();
}
} // namespace FFRT