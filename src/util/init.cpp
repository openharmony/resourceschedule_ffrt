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
#include "sched/stask_scheduler.h"
#include "eu/co_routine.h"
#include "eu/execute_unit.h"
#include "eu/sexecute_unit.h"
#include "dm/dependence_manager.h"
#include "dm/sdependence_manager.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/sysevent/sysevent.h"
#include "util/singleton_register.h"
#include "util/white_list.h"
#include "tm/task_factory.h"
#include "tm/io_task.h"
#include "tm/uv_task.h"
#include "tm/queue_task.h"
#include "util/slab.h"
#include "qos.h"
#ifdef FFRT_ENABLE_PERF_TRACE_SCOPED
#include "dfx/trace/ffrt_trace.h"
#endif
#ifdef FFRT_ASYNC_STACKTRACE
#include "dfx/async_stack/ffrt_async_stack.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif
void RegistCommonTaskFactory()
{
    ffrt::RegisterTaskFactoryCallbacks<ffrt::QueueTask>();
    ffrt::RegisterTaskFactoryCallbacks<ffrt::IOTask>();
    ffrt::RegisterTaskFactoryCallbacks<ffrt::UVTask>();
}

__attribute__((constructor)) static void ffrt_init()
{
    RegistCommonTaskFactory();

#ifdef FFRT_SEND_EVENT
    if (ffrt::IsBeta()) {
        ffrt::SetBetaVersionFlag();
    }
#endif

    ffrt::RegisterTaskFactoryCallbacks<ffrt::CPUEUTask, ffrt::SCPUEUTask>();
    ffrt::SchedulerFactory::RegistCb(
        [] () -> ffrt::TaskScheduler* { return new ffrt::STaskScheduler(); },
        [] (ffrt::TaskScheduler* schd) { delete schd; });
    CoRoutineFactory::RegistCb(
        [] (ffrt::CoTask* task, CoWakeType type) -> void {CoWake(task, type);});
    ffrt::DependenceManager::RegistInsCb(ffrt::SDependenceManager::Instance);
    ffrt::ExecuteUnit::RegistInsCb(ffrt::SExecuteUnit::Instance);
    ffrt::SetFuncQosMap(ffrt::QoSMap);
    ffrt::SetFuncQosMax(ffrt::QoSMax);
}
__attribute__((destructor)) static void FfrtDeinit(void)
{
}

void ffrt_child_init(void)
{
    ffrt_init();
}
#ifdef __cplusplus
}
#endif