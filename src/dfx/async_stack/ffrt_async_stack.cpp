/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "dfx/async_stack/ffrt_async_stack.h"

#include <cstdlib>
#include <mutex>

#include <dlfcn.h>

#include "dfx/log/ffrt_log_api.h"
namespace {
    CollectAsyncStackFunc g_collectAsyncStackFunc = nullptr;
    SetStackIdFunc g_setStackIdFunc = nullptr;
}

void FFRTSetAsyncStackFunc(CollectAsyncStackFunc collectAsyncStackFunc, SetStackIdFunc setStackIdFunc)
{
    const char* debuggableEnv = getenv("HAP_DEBUGGABLE");
    if (debuggableEnv != nullptr && strcmp(debuggableEnv, "true") == 0) {
        g_collectAsyncStackFunc = collectAsyncStackFunc;
        g_setStackIdFunc = setStackIdFunc;
    }
}

namespace ffrt {
uint64_t FFRTCollectAsyncStack(void)
{
    if (g_collectAsyncStackFunc != nullptr) {
        return g_collectAsyncStackFunc();
    }

    return 0;
}

void FFRTSetStackId(uint64_t stackId)
{
    if (g_setStackIdFunc != nullptr) {
        return g_setStackIdFunc(stackId);
    }
}
}