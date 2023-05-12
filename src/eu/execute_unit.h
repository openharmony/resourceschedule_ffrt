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

#ifndef FFRT_EXECUTE_UNIT_HPP
#define FFRT_EXECUTE_UNIT_HPP


#include "eu/workgroup_internal.h"
#include "eu/worker_manager.h"
#include "eu/thread_group.h"
#include "core/task_ctx.h"

namespace ffrt {
class ExecuteUnit {
public:
    static ExecuteUnit& Instance()
    {
        static ExecuteUnit eu;
        return eu;
    }

    ThreadGroup* BindTG(const DevType dev, QoS& qos);
    void UnbindTG(const DevType dev, QoS& qos);
    void BindWG(const DevType dev, QoS& qos);

    void NotifyTaskAdded(enum qos qos)
    {
        {
            wManager[static_cast<size_t>(DevType::CPU)]->NotifyTaskAdded(qos);
        }
    }

    std::mutex* GetSleepCtl(int qos)
    {
        return wManager[static_cast<size_t>(DevType::CPU)]->GetSleepCtl(qos);
    }

    WorkerGroupCtl* GetGroupCtl()
    {
        return wManager[static_cast<size_t>(DevType::CPU)]->GetGroupCtl();
    }

private:
    ExecuteUnit();

    std::array<std::unique_ptr<WorkerManager>, static_cast<size_t>(DevType::DEVMAX)> wManager;
};
} // namespace ffrt
#endif
