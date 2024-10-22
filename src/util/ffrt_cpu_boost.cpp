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
#ifndef OHOS_STANDARD_SYSTEM
#include "cpu_boost_adapter.h"
#else
#include "cpu_boost_ohos.h"
#endif
#include "cpp/task.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/osal.h"
#include "sched/execute_ctx.h"
#include "tm/cpu_task.h"
#include "c/ffrt_cpu_boost.h"

#ifdef __cplusplus
extern "C" {
#endif

API_ATTRIBUTE((visibility("default")))
void ffrt_cpu_boost_start(bool mode)
{
    int ret = ffrt::CpuBoostStart(ctx_id);
    if (ret == 0) {
        ffrt::CPUEUTask* curTask = ffrt::ExecuteCtx::Cur()->task;
        if (curTask != nullptr && curTask->cpuBoostCtxId < 0) {
            curTask->cpuBoostCtxId = ctx_id;
        }
    }
    return ret;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_cpu_boost_end(bool mode)
{
    int ret = ffrt::CpuBoostEnd(ctx_id);
    if (ret == 0) {
        ffrt::CPUEUTask* curTask = ffrt::ExecuteCtx::Cur()->task;
        if (curTask != nullptr && curTask->cpuBoostCtxId == ctx_id) {
            curTask->cpuBoostCtxId = -1;
        }
    }
    return ret;
}
#ifdef __cplusplus
}
#endif