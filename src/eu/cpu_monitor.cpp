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

#include "eu/cpu_monitor.h"
#include <iostream>
#include <thread>
#include <climits>
#include <unistd.h>
#include <securec.h>
#include "sched/scheduler.h"
#include "eu/execute_unit.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/sysevent/sysevent.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "internal_inc/config.h"
#include "internal_inc/osal.h"
#include "util/name_manager.h"
#include "sync/poller.h"
#include "util/ffrt_facade.h"
#include "util/spmc_queue.h"

namespace {
const size_t TIGGER_SUPPRESS_WORKER_COUNT = 4;
const size_t TIGGER_SUPPRESS_EXECUTION_NUM = 2;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
constexpr int JITTER_DELAY_MS = 5;
#endif
}

namespace ffrt {
CPUMonitor::CPUMonitor(CpuMonitorOps&& ops) : ops(ops)
{
    SetupMonitor();
}

CPUMonitor::~CPUMonitor()
{
    if (monitorThread != nullptr) {
        monitorThread->join();
    }
    delete monitorThread;
    monitorThread = nullptr;
}

void CPUMonitor::SetupMonitor()
{
    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        ctrlQueue[qos].hardLimit = DEFAULT_HARDLIMIT;
        ctrlQueue[qos].maxConcurrency = GlobalConfig::Instance().getCpuWorkerNum(qos);
        setWorkerMaxNum[qos] = false;
    }
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    memset_s(&domainInfoMonitor, sizeof(domainInfoMonitor), 0, sizeof(domainInfoMonitor));
    wakeupCond.check_ahead = false;
    wakeupCond.global.low = 0;
    wakeupCond.global.high = 0;
    for (int i = 0; i < BLOCKAWARE_DOMAIN_ID_MAX + 1; i++) {
        wakeupCond.local[i].low = 0;
        if (i < qosMonitorMaxNum) {
            wakeupCond.local[i].high = UINT_MAX;
            wakeupCond.global.low += wakeupCond.local[i].low;
            wakeupCond.global.high = UINT_MAX;
        } else {
            wakeupCond.local[i].high = 0;
        }
    }
#endif
}

void CPUMonitor::StartMonitor()
{
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    int ret = BlockawareInit(&keyPtr);
    if (ret != 0) {
        FFRT_LOGE("blockaware init fail, ret[%d], key[0x%lx]", ret, keyPtr);
    } else {
        blockAwareInit = true;
    }
#else
    monitorThread = nullptr;
#endif
}

int CPUMonitor::SetWorkerMaxNum(const QoS& qos, int num)
{
    WorkerCtrl& workerCtrl = ctrlQueue[qos()];
    workerCtrl.lock.lock();
    if (setWorkerMaxNum[qos()]) {
        FFRT_LOGE("qos[%d] worker num can only been setup once", qos());
        workerCtrl.lock.unlock();
        return -1;
    }
    if (num <= 0 || num > QOS_WORKER_MAXNUM) {
        FFRT_LOGE("qos[%d] worker num[%d] is invalid.", qos(), num);
        workerCtrl.lock.unlock();
        return -1;
    }
    workerCtrl.hardLimit = num;
    setWorkerMaxNum[qos()] = true;
    workerCtrl.lock.unlock();
    return 0;
}

uint32_t CPUMonitor::GetMonitorTid() const
{
    return monitorTid;
}

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
BlockawareWakeupCond* CPUMonitor::WakeupCond(void)
{
    return &wakeupCond;
}

void CPUMonitor::MonitorMain()
{
    (void)WorkerInit();
    int ret = BlockawareLoadSnapshot(keyPtr, &domainInfoMonitor);
    if (ret != 0) {
        FFRT_LOGE("blockaware load snapshot fail, ret[%d]", ret);
        return;
    }
    for (int i = 0; i < qosMonitorMaxNum; i++) {
        if (domainInfoMonitor.localinfo[i].nrRunning <= wakeupCond.local[i].low) {
            Notify(i, TaskNotifyType::TASK_ESCAPED);
        }
    }
    stopMonitor = true;
}

bool CPUMonitor::IsExceedRunningThreshold(const QoS& qos)
{
    return blockAwareInit && (BlockawareLoadSnapshotNrRunningFast(keyPtr, qos()) > ctrlQueue[qos()].maxConcurrency);
}

bool CPUMonitor::IsBlockAwareInit(void)
{
    return blockAwareInit;
}
#endif

void CPUMonitor::TimeoutCount(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    workerCtrl.sleepingWorkerNum--;
}

void CPUMonitor::WakeupSleep(const QoS& qos, bool irqWake)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    if (irqWake) {
        workerCtrl.irqEnable = false;
    }
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.executionNum++;
}

int CPUMonitor::TotalCount(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    int total = workerCtrl.sleepingWorkerNum + workerCtrl.executionNum;
    workerCtrl.lock.unlock();
    return total;
}

void CPUMonitor::RollbackDestroy(const QoS& qos, bool irqWake)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    if (irqWake) {
        workerCtrl.irqEnable = false;
    }
    workerCtrl.executionNum++;
}

bool CPUMonitor::TryDestroy(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    workerCtrl.sleepingWorkerNum--;
    return workerCtrl.sleepingWorkerNum > 0;
}

int CPUMonitor::SleepingWorkerNum(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::unique_lock lk(workerCtrl.lock);
    return workerCtrl.sleepingWorkerNum;
}

int CPUMonitor::WakedWorkerNum(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    return workerCtrl.executionNum;
}

void CPUMonitor::IntoDeepSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    workerCtrl.deepSleepingWorkerNum++;
}

void CPUMonitor::WakeupDeepSleep(const QoS& qos, bool irqWake)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    if (irqWake) {
        workerCtrl.irqEnable = false;
    }
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.deepSleepingWorkerNum--;
    workerCtrl.executionNum++;
}

void CPUMonitor::OutOfPollWait(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    workerCtrl.pollWaitFlag = false;
}

bool CPUMonitor::IsExceedDeepSleepThreshold()
{
    int totalWorker = 0;
    int deepSleepingWorkerNum = 0;
    for (unsigned int i = 0; i < static_cast<unsigned int>(QoS::Max()); i++) {
        WorkerCtrl& workerCtrl = ctrlQueue[i];
        std::lock_guard lk(workerCtrl.lock);
        deepSleepingWorkerNum += workerCtrl.deepSleepingWorkerNum;
        totalWorker += workerCtrl.executionNum + workerCtrl.sleepingWorkerNum;
    }
    return deepSleepingWorkerNum * 2 > totalWorker;
}

void CPUMonitor::Poke(const QoS& qos, uint32_t taskCount, TaskNotifyType notifyType)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    size_t runningNum = workerCtrl.executionNum;
    size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    /* There is no need to update running num when executionNum < maxConcurrency */
    if (workerCtrl.executionNum >= workerCtrl.maxConcurrency) {
        if (blockAwareInit) {
            auto nrBlocked = BlockawareLoadSnapshotNrBlockedFast(keyPtr, qos());
            if (workerCtrl.executionNum >= nrBlocked) {
                /* nrRunning may not be updated in a timely manner */
                runningNum = workerCtrl.executionNum - nrBlocked;
            } else {
                FFRT_LOGE("qos [%d] nrBlocked [%u] is larger than executionNum [%d].",
                    qos(), nrBlocked, workerCtrl.executionNum);
            }
        }
    }
#endif

    bool tiggerSuppression = (totalNum > TIGGER_SUPPRESS_WORKER_COUNT) &&
        (runningNum > TIGGER_SUPPRESS_EXECUTION_NUM) && (taskCount < runningNum);

    if (notifyType != TaskNotifyType::TASK_ADDED && notifyType != TaskNotifyType::TASK_ESCAPED && tiggerSuppression) {
        workerCtrl.lock.unlock();
        return;
    }
    if ((static_cast<uint32_t>(workerCtrl.sleepingWorkerNum) > 0) && (runningNum < workerCtrl.maxConcurrency)) {
        workerCtrl.lock.unlock();
        ops.WakeupWorkers(qos);
    } else if (((runningNum < workerCtrl.maxConcurrency) && (totalNum < workerCtrl.hardLimit)) || (runningNum == 0)) {
        workerCtrl.executionNum++;
        FFRTTraceRecord::WorkRecord((int)qos, workerCtrl.executionNum);
        workerCtrl.lock.unlock();
        ops.IncWorker(qos);
#ifdef FFRT_SEND_EVENT
        if (!((runningNum < workerCtrl.maxConcurrency) && (totalNum < workerCtrl.hardLimit))) {
            constexpr int processNameLen = 1024;
            static std::once_flag flag;
            static char processName[processNameLen];
            std::call_once(flag, []() { GetProcessName(processName, processNameLen); });
            WorkerEscapeReport(processName, static_cast<int>(qos), totalNum);
        }
#endif
    } else {
        if (workerCtrl.pollWaitFlag) {
            FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
        }
        workerCtrl.lock.unlock();
    }
}

void CPUMonitor::NotifyWorkers(const QoS& qos, int number)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();

    int increasableNumber = static_cast<int>(workerCtrl.maxConcurrency) -
        (workerCtrl.executionNum + workerCtrl.sleepingWorkerNum);
    int wakeupNumber = std::min(number, workerCtrl.sleepingWorkerNum);
    for (int idx = 0; idx < wakeupNumber; idx++) {
        ops.WakeupWorkers(qos);
    }

    int incNumber = std::min(number - wakeupNumber, increasableNumber);
    for (int idx = 0; idx < incNumber; idx++) {
        workerCtrl.executionNum++;
        ops.IncWorker(qos);
    }

    workerCtrl.lock.unlock();
    FFRT_LOGD("qos[%d] inc [%d] workers, wakeup [%d] workers", static_cast<int>(qos), incNumber, wakeupNumber);
}

// default strategy which is kind of radical for poking workers
void CPUMonitor::HandleTaskNotifyDefault(const QoS& qos, void* p, TaskNotifyType notifyType)
{
    CPUMonitor* monitor = reinterpret_cast<CPUMonitor*>(p);
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
void CPUMonitor::HandleTaskNotifyConservative(const QoS& qos, void* p, TaskNotifyType notifyType)
{
    CPUMonitor* monitor = reinterpret_cast<CPUMonitor*>(p);
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
        workerCtrl.lock.unlock();
    }
}

void CPUMonitor::HandleTaskNotifyUltraConservative(const QoS& qos, void* p, TaskNotifyType notifyType)
{
    (void)notifyType;
    CPUMonitor* monitor = reinterpret_cast<CPUMonitor*>(p);
    int taskCount = monitor->ops.GetTaskCount(qos);
    if (taskCount == 0) {
        // no available task in global queue, skip
        return;
    }

    WorkerCtrl& workerCtrl = monitor->ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lock(workerCtrl.lock);

    int runningNum = workerCtrl.executionNum;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    if (monitor->blockAwareInit) {
        /* nrBlocked may not be updated in a timely manner */
        auto nrBlocked = BlockawareLoadSnapshotNrBlockedFast(monitor->keyPtr, qos());
        runningNum = workerCtrl.executionNum - nrBlocked;
        if (!monitor->stopMonitor && taskCount == runningNum) {
            BlockawareWake();
            return;
        }
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
}