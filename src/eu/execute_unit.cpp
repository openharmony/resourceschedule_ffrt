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

#include "internal_inc/config.h"
#include "eu/cpuworker_manager.h"
#include "util/singleton_register.h"

namespace ffrt {
ExecuteUnit::ExecuteUnit()
{
}

ExecuteUnit& ExecuteUnit::Instance()
{
    return SingletonRegister<ExecuteUnit>::Instance();
}

void ExecuteUnit::RegistInsCb(SingleInsCB<ExecuteUnit>::Instance &&cb)
{
    SingletonRegister<ExecuteUnit>::RegistInsCb(std::move(cb));
}

ThreadGroup* ExecuteUnit::BindTG(const DevType dev, QoS& qos)
{
    return wManager[static_cast<size_t>(dev)]->JoinTG(qos);
}

void ExecuteUnit::BindWG(const DevType dev, QoS& qos)
{
    return wManager[static_cast<size_t>(dev)]->JoinRtg(qos);
}

void ExecuteUnit::UnbindTG(const DevType dev, QoS& qos)
{
    wManager[static_cast<size_t>(dev)]->LeaveTG(qos);
}
} // namespace ffrt
