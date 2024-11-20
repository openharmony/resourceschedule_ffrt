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

class CPUMonitor {
public:
    CPUMonitor(CpuMonitorOps&& ops);
    CPUMonitor(const CPUMonitor&) = delete;
    CPUMonitor& operator=(const CPUMonitor&) = delete;
    virtual ~CPUMonitor();
    uint32_t GetMonitorTid() const;
    int TotalCount(const QoS& qos);
    virtual void IntoSleep(const QoS& qos) = 0;

    void WakeupSleep(const QoS& qos, bool irqWake = false);
    void IntoDeepSleep(const QoS& qos);
    void WakeupDeepSleep(const QoS& qos, bool irqWake = false);
    void TimeoutCount(const QoS& qos);
    bool IsExceedDeepSleepThreshold();
    void IntoPollWait(const QoS& qos);
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
    int SetWorkerMaxNum(const QoS& qos, int num);
    /* strategy options for handling task notify events */
    static void HandleTaskNotifyDefault(const QoS& qos, void* p, TaskNotifyType notifyType);
    static void HandleTaskNotifyConservative(const QoS& qos, void* p, TaskNotifyType notifyType);
    static void HandleTaskNotifyUltraConservative(const QoS& qos, void* p, TaskNotifyType notifyType);
    int WakedWorkerNum(const QoS& qos);
    int SleepingWorkerNum(const QoS& qos);
    void NotifyWorkers(const QoS& qos, int number);
    void StartMonitor();

    CpuMonitorOps ops;
    std::thread* monitorThread = nullptr;
    uint32_t monitorTid = 0;
protected:
    WorkerCtrl ctrlQueue[QoS::MaxNum()];
    void Poke(const QoS& qos, uint32_t taskCount, TaskNotifyType notifyType);
    CpuMonitorOps& GetOps()
    {
        return ops;
    }
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    bool blockAwareInit = false;
    bool stopMonitor = false;
    unsigned long keyPtr = 0;
    int qosMonitorMaxNum = std::min(QoS::Max(), BLOCKAWARE_DOMAIN_ID_MAX + 1);
    BlockawareWakeupCond wakeupCond;
    BlockawareDomainInfoArea domainInfoMonitor;
    BlockawareDomainInfoArea domainInfoNotify;
    std::atomic<bool> exceedUpperWaterLine[QoS::MaxNum()];
#endif
private:
    void SetupMonitor();
    std::atomic<bool> setWorkerMaxNum[QoS::MaxNum()];
};
}
#endif /* CPU_MONITOR_H */