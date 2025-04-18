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

#ifndef FFRT_SCPUWORKER_MANAGER_HPP
#define FFRT_SCPUWORKER_MANAGER_HPP
#include "eu/cpuworker_manager.h"

namespace ffrt {
class SCPUWorkerManager : public CPUWorkerManager {
public:
    SCPUWorkerManager();
    ~SCPUWorkerManager() override;
    WorkerAction WorkerIdleAction(const WorkerThread* thread) override;
    TaskBase* PickUpTaskBatch(WorkerThread* thread) override;
    void WorkerRetiredSimplified(WorkerThread* thread) override;
    void WorkerPrepare(WorkerThread* thread) override;
    void WakeupWorkers(const QoS& qos) override;
private:
    void AddDelayedTask(int qos);
};
} // namespace ffrt
#endif