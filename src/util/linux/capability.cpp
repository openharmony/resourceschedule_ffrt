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

#include "util/capability.h"
#include <atomic>
#include <mutex>
#include <unistd.h>
namespace {
std::atomic<bool> g_exitFlag { false };
std::shared_mutex g_exitMtx;
}
namespace ffrt {
bool GetExitFlag()
{
    return g_exitFlag.load();
}

void SetExitFlag()
{
    g_exitFlag.store(true);
}

std::shared_mutex& GetExitMtx()
{
    return g_exitMtx;
}

void LockExitMtx()
{
    std::lock_guard lock(g_exitMtx);
}
bool CheckProcCapSysNice()
{
    return false;
}
} // ffrt