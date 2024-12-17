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

#include <cstring>

namespace ffrt {
const std::map<std::string, void(*)(const ffrt::QoS&, void*, ffrt::TaskNotifyType)> NOTIFY_FUNCTION_FACTORY = {
    { "CameraDaemon", ffrt::CPUMonitor::HandleTaskNotifyConservative },
    { "bluetooth", ffrt::CPUMonitor::HandleTaskNotifyUltraConservative },
};

WorkerThread* CPUManagerStrategy::CreateCPUWorker(const QoS& qos, void* manager)
{
    constexpr int processNameLen = 32;
    static std::once_flag flag;
    static char processName[processNameLen];
    std::call_once(flag, []() {
        GetProcessName(processName, processNameLen);
    });
    CPUWorkerManager* pIns = reinterpret_cast<CPUWorkerManager*>(manager);
    // default strategy of worker ops
    CpuWorkerOps ops {
        CPUWorker::WorkerLooperDefault,
        [pIns] (WorkerThread* thread) { return pIns->PickUpTaskFromGlobalQueue(thread); },
        [pIns] (const WorkerThread* thread) { pIns->NotifyTaskPicked(thread); },
        [pIns] (const WorkerThread* thread) { return pIns->WorkerIdleAction(thread); },
        [pIns] (WorkerThread* thread) { pIns->WorkerRetired(thread); },
        [pIns] (WorkerThread* thread) { pIns->WorkerPrepare(thread); },
        [pIns] (const WorkerThread* thread, int timeout) { return pIns->TryPoll(thread, timeout); },
        [pIns] (WorkerThread* thread) { return pIns->StealTaskBatch(thread); },
        [pIns] (WorkerThread* thread) { return pIns->PickUpTaskBatch(thread); },
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        [pIns] (const WorkerThread* thread) { return pIns->IsExceedRunningThreshold(thread); },
        [pIns] () { return pIns->IsBlockAwareInit(); },
#endif
    };

    if (strstr(processName, "CameraDaemon")) {
        // CameraDaemon customized strategy
#ifdef OHOS_STANDARD_SYSTEM
        ops.WorkerRetired = [pIns] (WorkerThread* thread) { pIns->WorkerRetiredSimplified(thread); };
#endif
    }

    return new (std::nothrow) CPUWorker(qos, std::move(ops), pIns);
}

CPUMonitor* CPUManagerStrategy::CreateCPUMonitor(void* manager)
{
    constexpr int processNameLen = 32;
    static std::once_flag flag;
    static char processName[processNameLen];
    std::call_once(flag, []() {
        GetProcessName(processName, processNameLen);
    });
    SCPUWorkerManager* pIns = reinterpret_cast<SCPUWorkerManager*>(manager);
    // default strategy of monitor ops
    CpuMonitorOps ops {
        [pIns] (const QoS& qos) { return pIns->IncWorker(qos); },
        [pIns] (const QoS& qos) { pIns->WakeupWorkers(qos); },
        [pIns] (const QoS& qos) { return pIns->GetTaskCount(qos); },
        [pIns] (const QoS& qos) { return pIns->GetWorkerCount(qos); },
        CPUMonitor::HandleTaskNotifyDefault,
    };

#ifdef OHOS_STANDARD_SYSTEM
    for (const auto& notifyFunc : NOTIFY_FUNCTION_FACTORY) {
        if (strstr(processName, notifyFunc.first.c_str())) {
            ops.HandleTaskNotity = notifyFunc.second;
            break;
        }
    }
#endif

    return new SCPUMonitor(std::move(ops));
}
}
