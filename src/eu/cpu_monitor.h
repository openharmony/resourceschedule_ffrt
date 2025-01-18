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

#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#include <atomic>
#include <vector>
#include <functional>
#include <mutex>
#include "qos.h"
#include "cpp/mutex.h"
#include "eu/cpu_manager_strategy.h"
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
#include "eu/blockaware.h"
#endif
#include "sync/sync.h"

namespace ffrt {
struct WorkerCtrl {
    alignas(cacheline_size) fast_mutex lock;
    alignas(cacheline_size) int executionNum = 0;
    alignas(cacheline_size) int sleepingWorkerNum = 0;
    alignas(cacheline_size) bool irqEnable = false;
    size_t hardLimit = 0;
    size_t maxConcurrency = 0;
    bool pollWaitFlag = false;
    int deepSleepingWorkerNum = 0;
    bool retryBeforeDeepSleep = true;
};

class EscapeMgr {
public:
    EscapeMgr(const std::function<void(int, void*)>& executeEscapeFunc) : executeEscapeFunc_(executeEscapeFunc)
    {
        for (int idx = 0; idx < QoS::MaxNum(); idx++) {
            we_[idx] = new WaitUntilEntry;
            we_[idx]->cb = nullptr;
        }
    }

    ~EscapeMgr()
    {
        FFRT_LOGI("Destructor.");
        for (int idx = 0; idx < QoS::MaxNum(); idx++) {
            if (we_[idx] != nullptr) {
                delete we_[idx];
                we_[idx] = nullptr;
            }
        }
    }

    int SetEscapeEnable(uint64_t oneStageIntervalMs, uint64_t twoStageIntervalMs,
        uint64_t threeStageIntervalMs, uint64_t oneStageWorkerNum, uint64_t twoStageWorkerNum)
    {
        if (enableEscape_) {
            FFRT_LOW("Worker escape is enabled, the interface cannot be invoked repeatedly.");
            return 1;
        }

        if (oneStageIntervalMs < oneStageIntervalMs_ || twoStageIntervalMs < twoStageIntervalMs_ ||
            threeStageIntervalMs < threeStageIntervalMs_ || oneStageWorkerNum > twoStageWorkerNum) {
            FFRT_LOGE("Setting failed, each stage interval value [%lu, %lu, %lu] "
            "cannot be smaller than default value [%lu, %lu, %lu], "
            "and one-stage worker number [%lu] cannot be larger than two-stage worker number [%lu].",
            oneStageIntervalMs, twoStageIntervalMs, threeStageIntervalMs, oneStageIntervalMs_,
            twoStageIntervalMs_, threeStageIntervalMs_, oneStageWorkerNum, twoStageWorkerNum);
            return 1;
        }

        enableEscape_ = true;
        oneStageIntervalMs_ = oneStageIntervalMs;
        twoStageIntervalMs_ = twoStageIntervalMs;
        threeStageIntervalMs_ = threeStageIntervalMs;
        oneStageWorkerNum_ = oneStageWorkerNum;
        twoStageWorkerNum_ = twoStageWorkerNum;
        FFRT_LOGI("Enable worker escape success, one-stage interval ms %lu, two-stage interval ms %lu, "
            "three-stage interval ms %lu, one-stage worker number %lu, two-stage worker number %lu.",
            oneStageIntervalMs_, twoStageIntervalMs_, threeStageIntervalMs_, oneStageWorkerNum_, twoStageWorkerNum_);
        return 0;
    }

    void SetEscapeDisable()
    {
        enableEscape_ = false;
        // after the escape function is disabled, parameters are restored to default values
        oneStageIntervalMs_ = 10;
        twoStageIntervalMs_ = 100;
        threeStageIntervalMs_ = 1000;
        oneStageWorkerNum_ = 128;
        twoStageWorkerNum_ = 256;
    }

    bool IsEscapeEnable()
    {
        return enableEscape_;
    }

    uint64_t CalEscapeInterval(uint64_t totalWorkerNum)
    {
        if (totalWorkerNum < oneStageWorkerNum_) {
            return oneStageIntervalMs_;
        } else if (totalWorkerNum >= oneStageWorkerNum_ && totalWorkerNum < twoStageWorkerNum_) {
            return twoStageIntervalMs_;
        } else {
            return threeStageIntervalMs_;
        }
    }

    void SubmitEscape(int qos, uint64_t totalWorkerNum, void* monitor)
    {
        // escape event has been triggered and will not be submitted repeatedly
        if (submittedDelayedTask_[qos]) {
            return;
        }

        we_[qos]->tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(CalEscapeInterval(totalWorkerNum));
        if (we_[qos]->cb == nullptr) {
            we_[qos]->cb = [this, qos, monitor] (WaitEntry* we) {
                (void)we;
                executeEscapeFunc_(qos, monitor);
                submittedDelayedTask_[qos] = false;
            };
        }

        if (!DelayedWakeup(we_[qos]->tp, we_[qos], we_[qos]->cb)) {
            FFRT_LOW("Failed to set qos %d escape task.", qos);
            return;
        }

        submittedDelayedTask_[qos] = true;
    }

private:
    bool enableEscape_ = false;
    uint64_t oneStageIntervalMs_ = 10;
    uint64_t twoStageIntervalMs_ = 100;
    uint64_t threeStageIntervalMs_ = 1000;
    uint64_t oneStageWorkerNum_ = 128;
    uint64_t twoStageWorkerNum_ = 256;

    bool submittedDelayedTask_[Qos::MaxNum()] = {0};
    WaitUntilEntry* we_[Qos::MaxNum()] = {nullptr};

    std::function<void(int, void*)> executeEscapeFunc_;
};

class CPUMonitor {
public:
    CPUMonitor(CpuMonitorOps&& ops, const std::function<void(int, void*)>& executeEscapeFunc);
    CPUMonitor(const CPUMonitor&) = delete;
    CPUMonitor& operator=(const CPUMonitor&) = delete;
    virtual ~CPUMonitor();
    uint32_t GetMonitorTid() const;
    int TotalCount(const QoS& qos);
    virtual void IntoSleep(const QoS& qos) = 0;
    virtual void IntoPollWait(const QoS& qos) = 0;

    void WakeupSleep(const QoS& qos, bool irqWake = false);
    void IntoDeepSleep(const QoS& qos);
    void WakeupDeepSleep(const QoS& qos, bool irqWake = false);
    void TimeoutCount(const QoS& qos);
    bool IsExceedDeepSleepThreshold();
    void OutOfPollWait(const QoS& qos);
    void RollbackDestroy(const QoS& qos, bool irqWake = false);
    bool TryDestroy(const QoS& qos);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    bool IsExceedRunningThreshold(const QoS& qos);
    bool IsBlockAwareInit(void);
    void MonitorMain();
    BlockawareWakeupCond* WakeupCond(void);
#endif
    virtual void Notify(const QoS& qos, TaskNotifyType notifyType) = 0;
    virtual void WorkerInit() = 0;
    int SetWorkerMaxNum(const QoS& qos, uint32_t num);
    int WakedWorkerNum(const QoS& qos);
    int SleepingWorkerNum(const QoS& qos);
    void NotifyWorkers(const QoS& qos, int number);
    void StartMonitor();
    int SetEscapeEnable(uint64_t oneStageIntervalMs, uint64_t twoStageIntervalMs,
        uint64_t threeStageIntervalMs, uint64_t oneStageWorkerNum, uint64_t twoStageWorkerNum);
    void SetEscapeDisable();

    CpuMonitorOps ops;
    std::thread* monitorThread = nullptr;
    uint32_t monitorTid = 0;

protected:
    size_t GetRunningNum(const QoS& qos);
    void ReportEscapeEvent(int qos, size_t totalNum);

    CpuMonitorOps& GetOps()
    {
        return ops;
    }

    WorkerCtrl ctrlQueue[QoS::MaxNum()];
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    bool blockAwareInit = false;
    bool stopMonitor = false;
    unsigned long keyPtr = 0;
    int qosMonitorMaxNum = std::min(QoS::Max(), BLOCKAWARE_DOMAIN_ID_MAX + 1);
    BlockawareWakeupCond wakeupCond;
    BlockawareDomainInfoArea domainInfoMonitor;
#endif
    EscapeMgr escapeMgr_;

private:
    void SetupMonitor();

    std::atomic<bool> setWorkerMaxNum[QoS::MaxNum()];
};
}
#endif /* CPU_MONITOR_H */
