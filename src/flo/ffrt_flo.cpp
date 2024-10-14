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
#ifdef OHOS_STANDARD_SYSTEM
#include "flo_ohos.h"
#else
#include "flo_adapter.h"
#endif
#include "cpp/task.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/osal.h"
#include "sched/execute_ctx.h"
#include "tm/cpu_task.h"

#ifdef __cplusplus
extern "C" {
#endif

API_ATTRIBUTE((visibility("default")))
int ffrt_flo_start(int ctx_id)
{
    int ret = ffrt::FloStart(ctx_id);
    if (ret == 0) {
        ffrt::CPUEUTask* curTask = ffrt::ExecuteCtx::Cur()->task;
        if (curTask != nullptr && curTask->floCtxId < 0) {
            curTask->floCtxId = ctx_id;
        }
    }
    return ret;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_flo_end(int ctx_id)
{
    int ret = ffrt::FloEnd(ctx_id);
    if (ret == 0) {
        ffrt::CPUEUTask* curTask = ffrt::ExecuteCtx::Cur()->task;
        if (curTask != nullptr && curTask->floCtxId == ctx_id) {
            curTask->floCtxId = -1;
        }
    }
    return ret;
}
#ifdef __cplusplus
}
#endif