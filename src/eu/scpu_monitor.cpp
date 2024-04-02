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
#include "eu/cpu_manager_interface.h"
#include "eu/scpu_monitor.h"

namespace ffrt {
SleepType SCPUMonitor::IntoSleep(const QoS& qos)
{
    SleepType type = SleepType::SLEEP_UNTIL_WAKEUP;
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum++;
    workerCtrl.executionNum--;
    workerCtrl.lock.unlock();
    return type;
}

void SCPUMonitor::Notify(const QoS& qos, TaskNotifyType notifyType)
{
    int taskCount = GetOps().GetTaskCount(qos);
    FFRT_LOGD("qos[%d] task notify op[%d] cnt[%d]", static_cast<int>(qos), static_cast<int>(notifyType),
        taskCount);
    switch (notifyType) {
        case TaskNotifyType::TASK_ADDED:
        case TaskNotifyType::TASK_PICKED:
            if (taskCount > 0) {
                Poke(qos, notifyType);
            }
            break;
#ifdef FFRT_IO_TASK_SCHEDULER
        case TaskNotifyType::TASK_LOCAL:
                Poke(qos, notifyType);
            break;
#endif
        default:
            break;
    }
}
}