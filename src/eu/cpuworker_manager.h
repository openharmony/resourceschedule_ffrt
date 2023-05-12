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

#ifndef FFRT_CPUWORKER_MANAGER_HPP
#define FFRT_CPUWORKER_MANAGER_HPP

#include "eu/worker_manager.h"
#include "eu/cpu_worker.h"
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_interface.h"


namespace ffrt {

constexpr int MANAGER_DESTRUCT_TIMESOUT = 1000000;

struct WorkerSleepCtl {
    std::mutex mutex;
    std::condition_variable cv;
};

class CPUWorkerManager : public WorkerManager {
public:
    CPUWorkerManager();

    ~CPUWorkerManager() override
    {
        tearDown = true;
        for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
            int try_cnt = MANAGER_DESTRUCT_TIMESOUT;
            while (try_cnt--) {
                sleepCtl[qos].cv.notify_all();
                {
                    usleep(1);
                    std::unique_lock lock(groupCtl[qos].tgMutex);
                    if (groupCtl[qos].threads.empty()) {
                        break;
                    }
                }
            }

            if (try_cnt <= 0) {
                FFRT_LOGE("erase qos[%d] threads failed", qos);
            }
        }
    }

    void NotifyTaskAdded(enum qos qos) override;
    void SetTidToCGroup(const std::string &path, const std::string &name, int32_t pid) override;

    std::mutex* GetSleepCtl(int qos) override
    {
        return &sleepCtl[qos].mutex;
    }

private:
    bool WorkerTearDown();
    bool IncWorker(const QoS& qos) override;
    bool DecWorker() override
    {return false;}
    void WakeupWorkers(const QoS& qos);
    int GetTaskCount(const QoS& qos);
    void WorkerRetired(WorkerThread* thread);
    TaskCtx* PickUpTask(WorkerThread* thread);
    void NotifyTaskPicked(const WorkerThread* thread);
    WorkerAction WorkerIdleAction(const WorkerThread* thread);
    void WorkerJoinTg(const QoS& qos, pid_t pid);
    void WorkerLeaveTg(const QoS& qos, pid_t pid);
    void WorkerSetup(WorkerThread* thread, const QoS& qos);

    CPUMonitor monitor;
    WorkerSleepCtl sleepCtl[QoS::Max()];
    bool tearDown = false;
};

} // namespace ffrt
#endif
