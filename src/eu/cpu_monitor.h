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
#include "eu/cpu_manager_interface.h"

namespace ffrt {
struct WorkerCtrl {
    size_t hardLimit = 0;
    size_t maxConcurrency = 0;
    size_t workerManagerID = 0;
    int executionNum = 0;
    int sleepingWorkerNum = 0;
#ifdef FFRT_IO_TASK_SCHEDULER
    bool pollWaitFlag = false;
#endif
    int deepSleepingWorkerNum = 0;
    bool hasWorkDeepSleep = 0;
    std::mutex lock;
};

class CPUMonitor {
public:
    CPUMonitor(CpuMonitorOps&& ops);
    CPUMonitor(const CPUMonitor&) = delete;
    CPUMonitor& operator=(const CPUMonitor&) = delete;
    virtual ~CPUMonitor();
    uint32_t GetMonitorTid() const;
    void HandleBlocked(const QoS& qos);
    void DecExeNumRef(const QoS& qos);
    void IncSleepingRef(const QoS& qos);
    void DecSleepingRef(const QoS& qos);
    virtual SleepType IntoSleep(const QoS& qos) = 0;
    virtual void WakeupCount(const QoS& qos, bool isDeepSleepWork = false);
    void IntoDeepSleep(const QoS& qos);
    void OutOfDeepSleep(const QoS& qos);
#ifdef FFRT_IO_TASK_SCHEDULER
    void IntoPollWait(const QoS& qos);
    void OutOfPollWait(const QoS& qos);
#endif
    void TimeoutCount(const QoS& qos);
    void RegWorker(const QoS& qos);
    void UnRegWorker();
    virtual void Notify(const QoS& qos, TaskNotifyType notifyType) = 0;
    int SetWorkerMaxNum(const QoS& qos, int num);
    bool IsExceedDeepSleepThreshold();
    int WakedWorkerNum(const QoS& qos);
    bool IsExceedMaxConcurrency(const QoS& qos);

    uint32_t monitorTid = 0;
protected:
    WorkerCtrl ctrlQueue[QoS::MaxNum()];
    void Poke(const QoS& qos, TaskNotifyType notifyType);
    CpuMonitorOps& GetOps()
    {
        return ops;
    }
private:
    size_t CountBlockedNum(const QoS& qos);
    void SetupMonitor();
    void StartMonitor();

    std::thread* monitorThread;
    CpuMonitorOps ops;
};
}
#endif /* CPU_MONITOR_H */
