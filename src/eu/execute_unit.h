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


#include "sched/workgroup_internal.h"
#include "eu/worker_manager.h"
#include "eu/thread_group.h"
#include "eu/cpu_monitor.h"
#include "internal_inc/osal.h"
#include "util/cb_func.h"

namespace ffrt {
class ExecuteUnit {
public:
    static ExecuteUnit& Instance();

    static void RegistInsCb(SingleInsCB<ExecuteUnit>::Instance &&cb);

    ThreadGroup* BindTG(const DevType dev, QoS& qos);
    void UnbindTG(const DevType dev, QoS& qos);
    void BindWG(const DevType dev, QoS& qos);

    void NotifyTaskAdded(const QoS& qos)
    {
        if (likely(wManager[static_cast<size_t>(DevType::CPU)])) {
            wManager[static_cast<size_t>(DevType::CPU)]->NotifyTaskAdded(qos);
        }
    }

    void NotifyWorkers(const QoS& qos, int number)
    {
        if (likely(wManager[static_cast<size_t>(DevType::CPU)])) {
            wManager[static_cast<size_t>(DevType::CPU)]->NotifyWorkers(qos, number);
        }
    }

    void NotifyLocalTaskAdded(const QoS& qos)
    {
        {
            wManager[static_cast<size_t>(DevType::CPU)]->NotifyLocalTaskAdded(qos);
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

    uint64_t GetWorkerNum()
    {
        return wManager[static_cast<size_t>(DevType::CPU)]->GetWorkerNum();
    }

    CPUMonitor* GetCPUMonitor()
    {
        return wManager[static_cast<size_t>(DevType::CPU)]->GetCPUMonitor();
    }

    virtual WorkerManager* InitManager() = 0;

    void CreateWorkerManager();

protected:
    ExecuteUnit();
    virtual ~ExecuteUnit();

    std::array<WorkerManager*, static_cast<size_t>(DevType::DEVMAX)> wManager;
};

} // namespace ffrt
#endif
