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
#include "eu/cpu_manager_strategy.h"
#include "eu/scpu_monitor.h"

namespace ffrt {
void SCPUMonitor::IntoSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum++;
    workerCtrl.executionNum--;
    workerCtrl.lock.unlock();
}

void SCPUMonitor::Notify(const QoS& qos, TaskNotifyType notifyType)
{
    GetOps().HandleTaskNotity(qos, this, notifyType);
}

void SCPUMonitor::WorkerInit()
{
    return;
}
}