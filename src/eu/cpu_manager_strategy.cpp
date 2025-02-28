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

#include "cpu_manager_strategy.h"
#include "internal_inc/osal.h"
#include "eu/cpuworker_manager.h"
#include "eu/scpuworker_manager.h"
#include "eu/scpu_monitor.h"
#include "util/ffrt_facade.h"
#include <cstring>

namespace ffrt {
std::array<sched_mode_type, QoS::MaxNum()> CPUManagerStrategy::schedMode {};

WorkerThread* CPUManagerStrategy::CreateCPUWorker(const QoS& qos, void* manager)
{
    CPUWorkerManager* pIns = reinterpret_cast<CPUWorkerManager*>(manager);
    // default strategy of worker ops
    CpuWorkerOps ops {
        [pIns] (const WorkerThread* thread) { pIns->NotifyTaskPicked(thread); },
        [pIns] (const WorkerThread* thread) { return pIns->WorkerIdleAction(thread); },
        [pIns] (WorkerThread* thread) { pIns->WorkerRetired(thread); },
        [pIns] (WorkerThread* thread) { pIns->WorkerPrepare(thread); },
        [pIns] (const WorkerThread* thread, int timeout) { return pIns->TryPoll(thread, timeout); },
        [pIns] (WorkerThread* thread) { return pIns->StealTaskBatch(thread); },
        [pIns] (WorkerThread* thread) { return pIns->PickUpTaskBatch(thread); },
        [pIns] (const QoS& qos) { return pIns->GetTaskCount(qos); },
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        [pIns] (const WorkerThread* thread) { return pIns->IsExceedRunningThreshold(thread); },
        [pIns] () { return pIns->IsBlockAwareInit(); },
#endif
    };
    if (strstr(GetCurrentProcessName(), "CameraDaemon")) {
        // CameraDaemon customized strategy
#ifdef OHOS_STANDARD_SYSTEM
        ops.WorkerRetired = [pIns] (WorkerThread* thread) { pIns->WorkerRetiredSimplified(thread); };
#endif
    }

    return new (std::nothrow) CPUWorker(qos, std::move(ops), pIns);
}

CPUMonitor* CPUManagerStrategy::CreateCPUMonitor(void* manager)
{
    SCPUWorkerManager* pIns = reinterpret_cast<SCPUWorkerManager*>(manager);
    // default strategy of monitor ops
    CpuMonitorOps ops {
        [pIns] (const QoS& qos) { return pIns->IncWorker(qos); },
        [pIns] (const QoS& qos) { pIns->WakeupWorkers(qos); },
        [pIns] (const QoS& qos) { return pIns->GetTaskCount(qos); },
        [pIns] (const QoS& qos) { return pIns->GetWorkerCount(qos); },
        SCPUMonitor::HandleTaskNotifyDefault,
    };
    return new SCPUMonitor(std::move(ops));
}
}