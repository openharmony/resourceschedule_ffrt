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
#include <unistd.h>
#include <securec.h>
#include "sched/scheduler.h"
#include "eu/execute_unit.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "internal_inc/config.h"
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
CPUMonitor::CPUMonitor(CpuMonitorOps&& ops)
    : ops(ops),
      qosWorkerConfig(QoS::MaxNum())
{
    SetupMonitor();
    StartMonitor();
}

CPUMonitor::~CPUMonitor()
{
    LogAllWorkerNum();
    if (monitorThread != nullptr) {
        monitorThread->join();
    }
    delete monitorThread;
    monitorThread = nullptr;
}

void CPUMonitor::SetupMonitor()
{
    globalReserveWorkerNum = DEFAULT_GLOBAL_RESERVE_NUM;
    lowQosReserveWorkerNum = DEFAULT_LOW_RESERVE_NUM;
    highQosReserveWorkerNum = DEFAULT_HIGH_RESERVE_NUM;

    globalReserveWorkerToken = std::make_unique<Token>(globalReserveWorkerNum);
    lowQosReserveWorkerToken = std::make_unique<Token>(lowQosReserveWorkerNum);
    highQosReserveWorkerToken = std::make_unique<Token>(highQosReserveWorkerNum);
    lowQosUseGlobalWorkerToken = std::make_unique<Token>(0);
    highQosUseGlobalWorkerToken = std::make_unique<Token>(0);

    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        ctrlQueue[qos].maxConcurrency = DEFAULT_MAXCONCURRENCY;
        if (qos > qos_max) {
            ctrlQueue[qos].hardLimit = DEFAULT_HARDLIMIT - DEFAULT_SINGLE_NUM;
            ctrlQueue[qos].reserveNum = 0;
            qosWorkerConfig.mQosWorkerCfg[qos].reserveNum = 0;
            qosWorkerConfig.mQosWorkerCfg[qos].hardLimit = DEFAULT_HARDLIMIT - DEFAULT_SINGLE_NUM;
            continue;
        }
        ctrlQueue[qos].hardLimit = DEFAULT_HARDLIMIT;
        ctrlQueue[qos].reserveNum = DEFAULT_SINGLE_NUM;
    }
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    memset_s(&domainInfoMonitor, sizeof(domainInfoMonitor), 0, sizeof(domainInfoMonitor));
    memset_s(&domainInfoNotify, sizeof(domainInfoNotify), 0, sizeof(domainInfoNotify));
    wakeupCond.check_ahead = false;
    wakeupCond.global.low = 0;
    wakeupCond.global.high = 0;
    for (int i = 0; i < BLOCKAWARE_DOMAIN_ID_MAX + 1; i++) {
        wakeupCond.local[i].low = 0;
        if (i < qosMonitorMaxNum) {
            wakeupCond.local[i].high = ctrlQueue[i].maxConcurrency;
            wakeupCond.global.low += wakeupCond.local[i].low;
            wakeupCond.global.high += wakeupCond.local[i].high;
        } else {
            wakeupCond.local[i].high = 0;
        }
    }
    for (int i = 0; i < QoS::MaxNum(); i++) {
        exceedUpperWaterLine[i] = false;
    }
#endif
}

void SetWorkerPara(unsigned int& param, unsigned int value)
{
    if (value != DEFAULT_PARAMS_VALUE) {
        param = value;
    }
}

int CPUMonitor::SetQosWorkerPara(ffrt_qos_config& qosCfg)
{
    SetWorkerPara(qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].maxConcurrency, qosCfg.maxConcurrency);
    SetWorkerPara(qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].hardLimit, qosCfg.hardLimit);
    SetWorkerPara(qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].reserveNum, qosCfg.reserveNum);

    if ((qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].maxConcurrency > MAX_MAXCONCURRENCY) ||
        (qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].hardLimit > GLOBAL_QOS_MAXNUM) ||
        (qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].reserveNum > GLOBAL_QOS_MAXNUM)) {
        FFRT_LOGE("qos[%d],maxConcurrency[%d],hardLimit[%d],reserveNum[%d] is invalid",
            qosCfg.qos, qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].maxConcurrency,
            qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].hardLimit,
            qosWorkerConfig.mQosWorkerCfg[qosCfg.qos].reserveNum);
        return -1;
    }
    return 0;
}

bool CPUMonitor::QosWorkerNumValid(ffrt_worker_num_param *qosData)
{
    bool setWorkerNumQos[QoS::MaxNum()] = {false};
    if (qosData->effectLen > QoS::MaxNum()) {
        FFRT_LOGE("effectLen is invalid[%d]", qosData->effectLen);
        return false;
    }

    for (unsigned int i = 0; i < qosData->effectLen; i++) {
        unsigned int qos = qosData->qosConfigArray[i].qos;
        if (qos >= QoS::MaxNum() || setWorkerNumQos[qos]) {
            FFRT_LOGE("qos[%d] is invalid or repeat setting", qos);
            return false;
        }
        setWorkerNumQos[qos] = true;
        if (SetQosWorkerPara(qosData->qosConfigArray[i] != 0)) {
            return false;
        }
    }

    SetWorkerPara(qosWorkerConfig.mLowQosReserveWorkerNum, qosData->lowQosReserveWorkerNum);
    SetWorkerPara(qosWorkerConfig.mHighQosReserveWorkerNum, qosData->highQosReserveWorkerNum);
    SetWorkerPara(qosWorkerConfig.mGlobalReserveWorkerNum, qosData->globalReserveWorkerNum);

    if ((qosWorkerConfig.mLowQosReserveWorkerNum> GLOBAL_QOS_MAXNUM) ||
        (qosWorkerConfig.mHighQosReserveWorkerNum > GLOBAL_QOS_MAXNUM) ||
        (qosWorkerConfig.mGlobalReserveWorkerNum > GLOBAL_QOS_MAXNUM)) {
        FFRT_LOGE("lowQosReserveWorkerNum[%d],highQosReserveWorkerNum[%d],globalReserveWorkerNum[%d]",
            qosWorkerConfig.mLowQosReserveWorkerNum, qosWorkerConfig.mHighQosReserveWorkerNum,
            qosWorkerConfig.mGlobalReserveWorkerNum);
        return false;
    }
    unsigned int totalReserveNum = qosWorkerConfig.GetGlobalMaxWorkerNum();
    if (totalReserveNum == 0 || totalReserveNum > GLOBAL_QOS_MAXNUM) {
        FFRT_LOGE("totalNum[%d],lowQosWorkerNum[%d],highQosWorkerNum[%d],globalWorkerNum[%d] invalid", totalReserveNum,
            qosData->lowQosReserveWorkerNum, qosData->highQosReserveWorkerNum, qosData->globalReserveWorkerNum);
        for (unsigned int i = 0; i < qosData->effectLen; i++) {
            ffrt_qos_config* singleQos = &(qosData->qosConfigArray[i]);
            FFRT_LOGE("totalReserveNum is check fail.reserveNum[%d]", singleQos->reserveNum);
        }
        return false;
    }
    return true;
}

bool CPUMonitor::MaxValueInvalid(unsigned int value, unsigned int default_value)
{
    return value != DEFAULT_PARAMS_VALUE && value > default_value;
}
template <typename T>
void CPUMonitor::Assignment(T& targetValue, unsigned int value)
{
    targetValue = value != DEFAULT_PARAMS_VALUE ? value : targetValue;
}

int CPUMonitor::QosWorkerNumSegment(ffrt_worker_num_param *qosData)
{
    setWorkerNumLock.lock();
    if (setWorkerNum) {
        setWorkerNumLock.unlock();
        FFRT_LOGE("qos config data setting repeat");
        return -1;
    }
    setWorkerNum = true;
    setWorkerNumLock.unlock();
    if (!QosWorkerNumValid(qosData)) {
        return -1;
    }
    for (int i = 0; i < QoS::MaxNum(); i++) {
        WorkerCtrl &workerCtrl = ctrlQueue[i];
        workerCtrl.lock.lock();
        if (workerCtrl.sleepingWorkerNum != 0 || workerCtrl.executionNum != 0) {
            for (int j = 0;j <= i; j++) {
                WorkerCtrl &workerCtrl = ctrlQueue[j];
                workerCtrl.lock.unlock();
            }
            FFRT_LOGE("Can only be set during initiallization,qos[%d], executionNum[%d],sleepingNum[%d]",
                i, workerCtrl.executionNum, workerCtrl.sleepingWorkerNum);
            return -1;
        }
    }

    for (int i = 0; i < QoS::MaxNum(); i++) {
        WorkerCtrl &workerCtrl = ctrlQueue[i];
        workerCtrl.hardLimit = qosWorkerConfig.mQosWorkerCfg[i].hardLimit;
        workerCtrl.maxConcurrency = qosWorkerConfig.mQosWorkerCfg[i].maxConcurrency;
        workerCtrl.reserveNum = qosWorkerConfig.mQosWorkerCfg[i].reserveNum;
    }

    lowQosReserveWorkerNum = qosWorkerConfig.mLowQosReserveWorkerNum;
    highQosReserveWorkerNum = qosWorkerConfig.mHighQosReserveWorkerNum;
    globalReserveWorkerNum = qosWorkerConfig.mGlobalReserveWorkerNum;
    globalReserveWorkerToken = std::make_unique<Token>(globalReserveWorkerNum);
    lowQosReserveWorkerToken = std::make_unique<Token>(lowQosReserveWorkerNum);
    highQosReserveWorkerToken = std::make_unique<Token>(highQosReserveWorkerNum);

    FFRT_LOGI("succ:globalReserveWorkerNum[%d],highQosReserveWorkerNum[%d],lowQosReserveWorkerNum[%d]",
        globalReserveWorkerNum, highQosReserveWorkerNum, lowQosReserveWorkerNum);
    for (int i = 0; i < QoS::MaxNum(); i++) {
        WorkerCtrl &workerCtrl = ctrlQueue[i];
        FFRT_LOGI("succ:qos[%d], reserveNum[%d], maxConcurrency[%d], hardLimit[%d]",
            i, workerCtrl.reserveNum, workerCtrl.maxConcurrency, workerCtrl.hardLimit);
        workerCtrl.lock.unlock();
    }
    return 0;
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
        size_t taskCount = static_cast<size_t>(ops.GetTaskCount(i));
        if (taskCount > 0 && domainInfoMonitor.localinfo[i].nrRunning <= wakeupCond.local[i].low) {
            Poke(i, taskCount, TaskNotifyType::TASK_ADDED);
        }
        if (domainInfoMonitor.localinfo[i].nrRunning > wakeupCond.local[i].high) {
            exceedUpperWaterLine[i] = true;
        }
    }
}

bool CPUMonitor::IsExceedRunningThreshold(const QoS& qos)
{
    if (blockAwareInit && exceedUpperWaterLine[qos()]) {
        exceedUpperWaterLine[qos()] = false;
        return true;
    }
    return false;
}

bool CPUMonitor::IsBlockAwareInit(void)
{
    return blockAwareInit;
}
#endif

void CPUMonitor::TimeoutCount(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);
    if (totalNum > workerCtrl.reserveNum) {
        ReleasePublicWorkerNum(qos);
    }
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.lock.unlock();
}

void CPUMonitor::WakeupCount(const QoS& qos, bool isDeepSleepWork)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.executionNum++;
    workerCtrl.lock.unlock();
}

void CPUMonitor::DoDestroy(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::unique_lock lk(workerCtrl.lock);
    size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);
    if (totalNum > workerCtrl.reserveNum) {
        ReleasePublicWorkerNum(qos);
    }
}

int CPUMonitor::WakedWorkerNum(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::unique_lock lk(workerCtrl.lock);
    return workerCtrl.executionNum;
}

bool CPUMonitor::HasDeepSleepWork(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::lock_guard lock(workerCtrl.lock);
    return workerCtrl.hasWorkDeepSleep;
}

void CPUMonitor::IntoDeepSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.deepSleepingWorkerNum++;
    workerCtrl.lock.unlock();
}

void CPUMonitor::OutOfDeepSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.executionNum++;
    workerCtrl.deepSleepingWorkerNum--;
    workerCtrl.lock.unlock();
}

void CPUMonitor::IntoPollWait(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.pollWaitFlag = true;
    workerCtrl.lock.unlock();
}

void CPUMonitor::OutOfPollWait(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.pollWaitFlag = false;
    workerCtrl.lock.unlock();
}

bool CPUMonitor::IsExceedDeepSleepThreshold()
{
    int totalWorker = 0;
    int deepSleepingWorkerNum = 0;
    for (unsigned int i = 0; i < static_cast<unsigned int>(QoS::Max()); i++) {
        WorkerCtrl& workerCtrl = ctrlQueue[i];
        workerCtrl.lock.lock();
        deepSleepingWorkerNum += workerCtrl.deepSleepingWorkerNum;
        totalWorker += workerCtrl.executionNum + workerCtrl.sleepingWorkerNum;
        workerCtrl.lock.unlock();
    }
    return deepSleepingWorkerNum * 2 > totalWorker;
}

bool CPUMonitor::LowQosUseReserveWorkerNum()
{
    if (lowQosReserveWorkerToken->try_acquire()) {
        return true;
    } else {
        if (globalReserveWorkerToken->try_acquire()) {
            lowQosUseGlobalWorkerToken->release();
            return true;
        } else {
            FFRT_LOGD("worker unavailable[%d], lowQosUse[%d], highQosUse[%d]",
                qos(), lowQosUseGlobalWorkerToken->load(), highQosUseGlobalWorkerToken->load());
            return false;
        }
    }
}

bool CPUMonitor::HighQosUseReserveWorkerNum()
{
    if (highQosReserveWorkerToken->try_acquire()) {
        return true;
    } else {
        if (globalReserveWorkerToken->try_acquire()) {
            highQosUseGlobalWorkerToken->release();
            return true;
        } else {
            FFRT_LOGD("worker unavailable[%d], lowQosUse[%d], highQosUse[%d]",
                qos(), lowQosUseGlobalWorkerToken->load(), highQosUseGlobalWorkerToken->load());
            return false;
        }
    }
}

bool CPUMonitor::TryAcquirePublicWorkerNum(const QoS& qos)
{
    return qos() <= ffrt_qos_default ? LowQosUseReserveWorkerNum() : HighQosUseReserveWorkerNum();
}

void CPUMonitor::ReleasePublicWorkerNum(const QoS& qos)
{
    if (qos() <= ffrt_qos_default) {
        if (lowQosUseGlobalWorkerToken->try_acquire()) {
            globalReserveWorkerToken->release();
        } else {
            lowQosReserveWorkerToken->release();
        }
    } else {
        if (highQosUseGlobalWorkerToken->try_acquire()) {
            globalReserveWorkerToken->release();
        } else {
            highQosReserveWorkerToken->release();
        }
    }
}

void CPUMonitor::LogAllWorkerNum()
{
    FFRT_LOGD("globalReserveWorkerNum[%d],highQosReserveWorkerNum[%d],lowQosReserveWorkerNum[%d]",
        globalReserveWorkerNum, highQosReserveWorkerNum, lowQosReserveWorkerNum);
    FFRT_LOGD("globalReserveWorkerToken[%d],highQosReserveWorkerToken[%d],lowQosReserveWorkerToken[%d]",
        globalReserveWorkerToken->load(), highQosReserveWorkerToken->load(), lowQosReserveWorkerToken->load());
    FFRT_LOGD("lowQosUseGlobalWorkerToken[%d], highQosUseGlobalWorkerToken[%d]",
        lowQosUseGlobalWorkerToken->load(), highQosUseGlobalWorkerToken->load());
    for (int i = 0; i < QoS::MaxNum(); i++) {
        WorkerCtrl &workerCtrl = ctrlQueue[i];
        size_t runningNum = workerCtrl.executionNum;
        size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);
        FFRT_LOGD("succ:qos[%d], reserveNum[%d], maxConcurrency[%d], hardLimit[%d], runningNum[%d], totalNum[%d]",
            i, workerCtrl.reserveNum, workerCtrl.maxConcurrency, workerCtrl.hardLimit, runningNum, totalNum);
    }
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
        if (blockAwareInit && !BlockawareLoadSnapshot(keyPtr, &domainInfoNotify)) {
            /* nrRunning may not be updated in a timely manner */
            runningNum = workerCtrl.executionNum - domainInfoNotify.localinfo[qos()].nrBlocked;
        }
    }
#endif

    bool tiggerSuppression = (totalNum > TIGGER_SUPPRESS_WORKER_COUNT) &&
        (runningNum > TIGGER_SUPPRESS_EXECUTION_NUM) && (taskCount < runningNum);

    if (notifyType != TaskNotifyType::TASK_ADDED && tiggerSuppression) {
        workerCtrl.lock.unlock();
        return;
    }
    if (static_cast<uint32_t>(workerCtrl.sleepingWorkerNum) > 0) {
        workerCtrl.lock.unlock();
        ops.WakeupWorkers(qos);
    } else if ((runningNum < workerCtrl.maxConcurrency) && (totalNum < workerCtrl.hardLimit) &&
        (totalNum < workerCtrl.reserveNum || TryAcquirePublicWorkerNum(qos))) {
        workerCtrl.executionNum++;
        FFRTTraceRecord::WorkRecord(static_cast<int>(qos), workerCtrl.executionNum);
        workerCtrl.lock.unlock();
        ops.IncWorker(qos);
    } else {
        if (workerCtrl.pollWaitFlag) {
            FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
        }
        FFRT_LOGD("noInc:qos[%d],reserveNum[%d],maxConcurrency[%d],hardLimit[%d],runningNum[%d],totalNum[%d]",
            qos(), workerCtrl.reserveNum, workerCtrl.maxConcurrency, workerCtrl.hardLimit, runningNum, totalNum);
        workerCtrl.lock.unlock();
    }
}

void CPUMonitor::NotifyWorkers(const QoS& qos, int number)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();

    int maxWorkerLimit = static_cast<int>(std::min(workerCtrl.maxConcurrency, workerCtrl.hardLimit));
    int increasableNumber = maxWorkerLimit - (workerCtrl.executionNum + workerCtrl.sleepingWorkerNum);
    int wakeupNumber = std::min(number, workerCtrl.sleepingWorkerNum);
    for (int idx = 0; idx < wakeupNumber; idx++) {
        ops.WakeupWorkers(qos);
    }

    int incPublicNum = workerCtrl.reserveNum - (workerCtrl.executionNum + workerCtrl.sleepingWorkerNum);
    int incNumber = std::min(number - wakeupNumber, increasableNumber);
    for (int idx = 0; idx < incNumber; idx++) {
        if (idx < incPublicNum || TryAcquirePublicWorkerNum(qos)) {
            workerCtrl.executionNum++;
            ops.IncWorker(qos);
        } else {
            FFRT_LOGD("Fail:qos[%d],reserveNum[%d],maxConcurrency[%d],hardLimit[%d],totalNum[%d],idx[%d],inc[%d]",
                qos(), workerCtrl.reserveNum, workerCtrl.maxConcurrency, workerCtrl.hardLimit,
                workerCtrl.executionNum + workerCtrl.sleepingWorkerNum, idx, incNumber);
        }
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


void CPUMonitor::PokeAdd(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    if (static_cast<uint32_t>(workerCtrl.sleepingWorkerNum) > 0) {
        workerCtrl.lock.unlock();
        return;
    } else {
        size_t runningNum = workerCtrl.executionNum;
        size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        if (workerCtrl.executionNum >= workerCtrl.maxConcurrency) {
            if (blockAwareInit && !BlockawareLoadSnapshot(keyPtr, &domainInfoNotify)) {
                runningNum = workerCtrl.executionNum - domainInfoNotify.localinfo[qos()].nrBlocked;
            }
        }
#endif
        if ((runningNum < workerCtrl.maxConcurrency) && (totalNum < workerCtrl.hardLimit) &&
        (totalNum < workerCtrl.reserveNum || TryAcquirePublicWorkerNum(qos))) {
            workerCtrl.executionNum++;
            workerCtrl.lock.unlock();
            ops.IncWorker(qos);
        } else {
            if (workerCtrl.pollWaitFlag) {
                FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
            }
            FFRT_LOGD("noInc:qos[%d],reserveNum[%d],maxConcurrency[%d],hardLimit[%d],runningNum[%d],totalNum[%d]",
                qos(), workerCtrl.reserveNum, workerCtrl.maxConcurrency, workerCtrl.hardLimit, runningNum, totalNum);
            workerCtrl.lock.unlock();
        }
    }
}

void CPUMonitor::PokePick(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    if (static_cast<uint32_t>(workerCtrl.sleepingWorkerNum) > 0) {
        if (workerCtrl.hasWorkDeepSleep &&GetOps().GetTaskCount(qos) == 0) {
            workerCtrl.lock.unlock();
            return;
        }

        workerCtrl.lock.unlock();

        ops.WakeupWorkers(qos);
    } else {
        size_t runningNum = workerCtrl.executionNum;
        size_t totalNum = static_cast<size_t>(workerCtrl.sleepingWorkerNum + workerCtrl.executionNum);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        if (workerCtrl.executionNum >= workerCtrl.maxConcurrency) {
            if (blockAwareInit && !BlockawareLoadSnapshot(keyPtr, &domainInfoNotify)) {
                runningNum = workerCtrl.executionNum - domainInfoNotify.localinfo[qos()].nrBlocked;
            }
        }
#endif
        if ((runningNum < workerCtrl.maxConcurrency) && (totalNum < workerCtrl.hardLimit) &&
        (totalNum < workerCtrl.reserveNum || TryAcquirePublicWorkerNum(qos))) {
            workerCtrl.executionNum++;
            workerCtrl.lock.unlock();
            ops.IncWorker(qos);
        } else {
            if (workerCtrl.pollWaitFlag) {
                FFRTFacade::GetPPInstance().GetPoller(qos).WakeUp();
            }
            FFRT_LOGD("noInc:qos[%d],reserveNum[%d],maxConcurrency[%d],hardLimit[%d],runningNum[%d],totalNum[%d]",
                qos(), workerCtrl.reserveNum, workerCtrl.maxConcurrency, workerCtrl.hardLimit, runningNum, totalNum);
            workerCtrl.lock.unlock();
        }
    }
}

}