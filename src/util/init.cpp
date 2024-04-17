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
#include <dlfcn.h>
#include "sched/task_scheduler.h"
#include "eu/co_routine.h"
#include "eu/execute_unit.h"
#include "eu/sexecute_unit.h"
#include "dm/dependence_manager.h"
#include "dm/sdependence_manager.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/singleton_register.h"
#include "tm/task_factory.h"
#include "qos.h"

#ifdef __cplusplus
extern "C" {
#endif
__attribute__((constructor)) static void ffrt_init()
{
    ffrt::TaskFactory::RegistCb(
        [] () -> ffrt::CPUEUTask* {
            return static_cast<ffrt::CPUEUTask*>(ffrt::SimpleAllocator<ffrt::SCPUEUTask>::allocMem());
        },
        [] (ffrt::CPUEUTask* task) {
            ffrt::SimpleAllocator<ffrt::SCPUEUTask>::FreeMem(static_cast<ffrt::SCPUEUTask*>(task));
    });
    ffrt::SchedulerFactory::RegistCb(
        [] () -> ffrt::TaskScheduler* { return new ffrt::SFIFOScheduler; },
        [] (ffrt::TaskScheduler* schd) { delete schd; });
    CoRoutineFactory::RegistCb(
        [] (ffrt::CPUEUTask* task, bool timeOut) -> void {CoWake(task, timeOut);});
    ffrt::DependenceManager::RegistInsCb(ffrt::SDependenceManager::Instance);
    ffrt::ExecuteUnit::RegistInsCb(ffrt::SExecuteUnit::Instance);
    ffrt::FFRTScheduler::RegistInsCb(ffrt::SFFRTScheduler::Instance);
    ffrt::SetFuncQosMap(ffrt::QoSMap);
    ffrt::SetFuncQosMax(ffrt::QoSMax);
}
#ifdef __cplusplus
}
#endif