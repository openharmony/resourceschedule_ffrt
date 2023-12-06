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
#include "execute_ctx.h"

namespace ffrt {

ExecuteCtx::ExecuteCtx()
{
    task = nullptr;
#ifdef FFRT_IO_TASK_SCHEDULER
    exec_task = nullptr;
    local_fifo = nullptr;
    priority_task_ptr = nullptr;
#endif
    wn.weType = 2;
}

ExecuteCtx::~ExecuteCtx()
{
}

ExecuteCtx* ExecuteCtx::Cur()
{
    thread_local static ExecuteCtx ctx;
    return &ctx;
}

} // namespace ffrt