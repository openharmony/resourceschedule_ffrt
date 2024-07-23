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

#include "eu/execute_unit.h"
#include "eu/sexecute_unit.h"

#include "internal_inc/config.h"
#include "eu/scpuworker_manager.h"
#include "eu/co_routine_factory.h"

namespace ffrt {
std::unique_ptr<WorkerManager> SExecuteUnit::InitManager()
{
    return std::unique_ptr<WorkerManager>(new (std::nothrow) SCPUWorkerManager());
}
SExecuteUnit::SExecuteUnit()
{
    ExecuteUnit::CreateWorkerManager();
}
} // namespace ffrt
