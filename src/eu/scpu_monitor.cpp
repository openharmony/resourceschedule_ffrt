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

#include "eu/scpu_monitor.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "eu/cpu_manager_strategy.h"
#include "util/ffrt_facade.h"

namespace {
const size_t TIGGER_SUPPRESS_WORKER_COUNT = 4;
const size_t TIGGER_SUPPRESS_EXECUTION_NUM = 2;
const size_t MAX_ESCAPE_WORKER_NUM = 1024;
}

namespace ffrt {
void SCPUMonitor::IntoSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum++;
    workerCtrl.executionNum--;
    workerCtrl.lock.unlock();
}

void SCPUMonitor::IntoPollWait(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    workerCtrl.pollWaitFlag = true;
}

void SCPUMonitor::Notify(const QoS& qos, TaskNotifyType notifyType)
{
    GetOps().HandleTaskNotity(qos, this, notifyType);
}

void SCPUMonitor::WorkerInit()
{
    return;
}

// default strategy which is kind of radical for poking workers
void SCPUMonitor::HandleTaskNotifyDefault(const QoS& qos, void* monitorPtr, TaskNotifyType notifyType)
{
    SCPUMonitor* monitor = reinterpret_cast<SCPUMonitor*>(monitorPtr);
    size_t taskCount = static_cast<size_t>(monitor->GetOps().GetTaskCount(qos));
    switch (notifyType) {
        case TaskNotifyType::TASK_ADDED:
        case TaskNotifyType::TASK_PICKED:
        case TaskNotifyType::TASK_ESCAPED:
            if (taskCount > 0) {
                monitor->Poke(qos, taskCount, notifyType);
            }
            break;
        case TaskNotifyType::TASK_LOCAL:
                monitor->Poke(qos, taskCount, notifyType);
            break;
        default:
            break;
    }
}

// conservative strategy for poking workers
void SCPUMonitor::HandleTaskNotifyConservative(const QoS& qos, void* monitorPtr, TaskNotifyType notifyType)
{
    SCPUMonitor* monitor = reinterpret_cast<SCPUMonitor*>(monitorPtr);
    int taskCount = monitor->ops.GetTaskCount(qos);
    if (taskCount == 0) {
        // no available task in global queue, skip
        return;
    }
    constexpr double thresholdTaskPick = 1.0;
    WorkerCtrl& workerCtrl = monitor->ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();

    if (notifyType == TaskNotifyType::TASK_PICKED) {
        int wakedWorkerCount = workerCtrl.executionNum;
        double remainingLoadRatio = (wakedWorkerCount == 0) ? static_cast<double>(workerCtrl.maxConcurrency) :
            static_cast<double>(taskCount) / static_cast<double>(wakedWorkerCount);
        if (remainingLoadRatio <= thresholdTaskPick) {
            // for task pick, wake worker when load ratio > 1
            workerCtrl.lock.unlock();
            return;
        }
    }

    if (static_cast<uint32_t>(workerCtrl.executionNum) < workerCtrl.maxConcurrency) {
        if (workerCtrl.sleepingWorkerNum == 0) {
            FFRT_LOGI("begin to create worker, notifyType[%d]"
                "execnum[%d], maxconcur[%d], slpnum[%d], dslpnum[%d]",
                notifyType, workerCtrl.executionNum, workerCtrl.maxConcurrency,
                workerCtrl.sleepingWorkerNum, workerCtrl.deepSleepingWorkerNum);
            workerCtrl.executionNum++;
            workerCtrl.lock.unlock();
            monitor->ops.IncWorker(qos);
        } else {
            workerCtrl.lock.unlock();
            monitor->ops.WakeupWorkers(qos);
        }
    } else {
        if (workerCtrl.pollWaitFlag) {
            FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
        }
        workerCtrl.lock.unlock();
    }
}

void SCPUMonitor::HandleTaskNotifyUltraConservative(const QoS& qos, void* monitorPtr, TaskNotifyType notifyType)
{
    (void)notifyType;
    SCPUMonitor* monitor = reinterpret_cast<SCPUMonitor*>(monitorPtr);
    int taskCount = monitor->ops.GetTaskCount(qos);
    if (taskCount == 0) {
        // no available task in global queue, skip
        return;
    }

    WorkerCtrl& workerCtrl = monitor->ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lock(workerCtrl.lock);

    int runningNum = static_cast<int>(monitor->GetRunningNum(qos));
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    if (monitor->blockAwareInit && !monitor->stopMonitor && taskCount == runningNum) {
        return;
    }
#endif

    if (taskCount < runningNum) {
        return;
    }

    if (runningNum < static_cast<int>(workerCtrl.maxConcurrency)) {
        if (workerCtrl.sleepingWorkerNum == 0) {
            workerCtrl.executionNum++;
            monitor->ops.IncWorker(qos);
        } else {
            monitor->ops.WakeupWorkers(qos);
        }
    }
}

void SCPUMonitor::Poke(const QoS& qos, uint32_t taskCount, TaskNotifyType notifyType)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    size_t runningNum = GetRunningNum(qos);
    size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);

    bool tiggerSuppression = (totalNum > TIGGER_SUPPRESS_WORKER_COUNT) &&
        (runningNum > TIGGER_SUPPRESS_EXECUTION_NUM) && (taskCount < runningNum);
    if (notifyType != TaskNotifyType::TASK_ADDED && notifyType != TaskNotifyType::TASK_ESCAPED && tiggerSuppression) {
        workerCtrl.lock.unlock();
        return;
    }

    if ((static_cast<uint32_t>(workerCtrl.sleepingWorkerNum) > 0) && (runningNum < workerCtrl.maxConcurrency)) {
        workerCtrl.lock.unlock();
        ops.WakeupWorkers(qos);
    } else if ((runningNum < workerCtrl.maxConcurrency) && (totalNum < workerCtrl.hardLimit)) {
        workerCtrl.executionNum++;
        FFRTTraceRecord::WorkRecord(qos(), workerCtrl.executionNum);
        workerCtrl.lock.unlock();
        ops.IncWorker(qos);
    } else if (escapeMgr_.IsEscapeEnable() && (runningNum == 0) && (totalNum < MAX_ESCAPE_WORKER_NUM)) {
        escapeMgr_.SubmitEscape(qos, totalNum, this);
        workerCtrl.lock.unlock();
    } else {
        if (workerCtrl.pollWaitFlag) {
            FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
        }
        workerCtrl.lock.unlock();
    }
}

void SCPUMonitor::ExecuteEscape(int qos, void* monitorPtr)
{
    SCPUMonitor* monitor = reinterpret_cast<SCPUMonitor*>(monitorPtr);
    if (monitor->escapeMgr_.IsEscapeEnable() && monitor->ops.GetTaskCount(qos) > 0) {
        WorkerCtrl& workerCtrl = monitor->ctrlQueue[qos];
        workerCtrl.lock.lock();

        size_t runningNum = monitor->GetRunningNum(qos);
        size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);
        if ((workerCtrl.sleepingWorkerNum > 0) && (runningNum < workerCtrl.maxConcurrency)) {
            workerCtrl.lock.unlock();
            monitor->ops.WakeupWorkers(qos);
        } else if ((runningNum == 0) && (totalNum < MAX_ESCAPE_WORKER_NUM)) {
            workerCtrl.executionNum++;
            FFRTTraceRecord::WorkRecord(qos, workerCtrl.executionNum);
            workerCtrl.lock.unlock();
            monitor->ops.IncWorker(qos);
            monitor->ReportEscapeEvent(qos, totalNum);
        } else {
            if (workerCtrl.pollWaitFlag) {
                FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
            }
            workerCtrl.lock.unlock();
        }
    }
}
}