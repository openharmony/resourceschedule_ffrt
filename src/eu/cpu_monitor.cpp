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
#include "internal_inc/config.h"
#include "internal_inc/osal.h"
#include "util/name_manager.h"
#include "sync/poller.h"
#include "util/ffrt_facade.h"
#include "util/spmc_queue.h"

namespace {
const size_t MAX_ESCAPE_WORKER_NUM = 1024;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
constexpr int JITTER_DELAY_MS = 5;
#endif
}

namespace ffrt {
CPUMonitor::CPUMonitor(CpuMonitorOps&& ops, const std::function<void(int, void*)>& executeEscapeFunc)
    : ops(ops), escapeMgr_(executeEscapeFunc)
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
        FFRT_SYSEVENT_LOGE("blockaware init fail, ret[%d], key[0x%lx]", ret, keyPtr);
    } else {
        blockAwareInit = true;
    }
#else
    monitorThread = nullptr;
#endif
}

int CPUMonitor::SetWorkerMaxNum(const QoS& qos, uint32_t num)
{
    WorkerCtrl& workerCtrl = ctrlQueue[qos()];
    std::lock_guard lk(workerCtrl.lock);
    if (setWorkerMaxNum[qos()]) {
        FFRT_SYSEVENT_LOGE("qos[%d] worker num can only been setup once", qos());
        return -1;
    }

    workerCtrl.hardLimit = static_cast<size_t>(num);
    setWorkerMaxNum[qos()] = true;
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
        FFRT_SYSEVENT_LOGE("blockaware load snapshot fail, ret[%d]", ret);
        return;
    }
    for (int i = 0; i < qosMonitorMaxNum; i++) {
        auto& info = domainInfoMonitor.localinfo[i];
        if (info.nrRunning <= wakeupCond.local[i].low &&
            (info.nrRunning + info.nrBlocked + info.nrSleeping) < MAX_ESCAPE_WORKER_NUM) {
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

int CPUMonitor::SetEscapeEnable(uint64_t oneStageIntervalMs, uint64_t twoStageIntervalMs,
        uint64_t threeStageIntervalMs, uint64_t oneStageWorkerNum, uint64_t twoStageWorkerNum)
{
    return escapeMgr_.SetEscapeEnable(oneStageIntervalMs, twoStageIntervalMs,
        threeStageIntervalMs, oneStageWorkerNum, twoStageWorkerNum);
}

void CPUMonitor::SetEscapeDisable()
{
    escapeMgr_.SetEscapeDisable();
}

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
    if (workerCtrl.pendingWakeCnt > 0) {
        workerCtrl.pendingWakeCnt--;
    }
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.executionNum++;
}

int CPUMonitor::TotalCount(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);
    int total = workerCtrl.sleepingWorkerNum + workerCtrl.executionNum;
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
    std::lock_guard lk(workerCtrl.lock);
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

void CPUMonitor::NotifyWorkers(const QoS& qos, int number)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lk(workerCtrl.lock);

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

    FFRT_LOGD("qos[%d] inc [%d] workers, wakeup [%d] workers", static_cast<int>(qos), incNumber, wakeupNumber);
}

size_t CPUMonitor::GetRunningNum(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[qos()];
    size_t runningNum = workerCtrl.executionNum;

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    /* There is no need to update running num when executionNum < maxConcurrency */
    if (workerCtrl.executionNum >= workerCtrl.maxConcurrency && blockAwareInit) {
        auto nrBlocked = BlockawareLoadSnapshotNrBlockedFast(keyPtr, qos());
        if (workerCtrl.executionNum >= nrBlocked) {
            /* nrRunning may not be updated in a timely manner */
            runningNum = workerCtrl.executionNum - nrBlocked;
        } else {
            FFRT_SYSEVENT_LOGE("qos [%d] nrBlocked [%u] is larger than executionNum [%d].",
                qos(), nrBlocked, workerCtrl.executionNum);
        }
    }
#endif

    return runningNum;
}

void CPUMonitor::ReportEscapeEvent(int qos, size_t totalNum)
{
#ifdef FFRT_SEND_EVENT
    WorkerEscapeReport(GetCurrentProcessName(), qos, totalNum);
#endif
}
}
