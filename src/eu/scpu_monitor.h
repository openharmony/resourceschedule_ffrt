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

#ifndef SCPU_MONITOR_H
#define SCPU_MONITOR_H

#include "eu/cpu_manager_strategy.h"
#include "eu/cpu_monitor.h"

namespace ffrt {

class SCPUMonitor : public CPUMonitor {
public:
    SCPUMonitor(CpuMonitorOps&& ops) : CPUMonitor(std::move(ops), ExecuteEscape) {}
    void IntoSleep(const QoS& qos) override;
    void IntoPollWait(const QoS& qos) override;
    void Notify(const QoS& qos, TaskNotifyType notifyType) override;
    void WorkerInit() override;

    /* strategy options for handling task notify events */
    static void HandleTaskNotifyDefault(const QoS& qos, void* monitorPtr, TaskNotifyType notifyType);
    static void HandleTaskNotifyConservative(const QoS& qos, void* monitorPtr, TaskNotifyType notifyType);
    static void HandleTaskNotifyUltraConservative(const QoS& qos, void* monitorPtr, TaskNotifyType notifyType);

private:
    void Poke(const QoS& qos, uint32_t taskCount, TaskNotifyType notifyType);

    static void ExecuteEscape(int qos, void* monitorPtr);
};
}
#endif /* SCPU_MONITOR_H */