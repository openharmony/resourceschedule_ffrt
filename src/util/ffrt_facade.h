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
#ifndef UTIL_FFRTFACADE_HPP
#define UTIL_FFRTFACADE_HPP
#include <functional>
#include <mutex>
#include "internal_inc/non_copyable.h"
#include "sched/scheduler.h"
#include "eu/co_routine.h"
#include "eu/execute_unit.h"
#include "dm/dependence_manager.h"
#include "queue/queue_monitor.h"
#include "sync/delayed_worker.h"
#include "eu/io_poller.h"
#include "sync/timer_manager.h"
#include "util/worker_monitor.h"
#include "dfx/trace/ffrt_trace_chain.h"
#include "dfx/trace/ffrt_trace.h"

namespace ffrt {
bool GetInitFlag();
const char* GetCurrentProcessName();
bool GetDelayedWorkerExitFlag();
void SetDelayedWorkerExitFlag();
bool GetBetaVersionFlag();
void SetBetaVersionFlag();

// global scheduler mutexs per qos
inline std::mutex g_schedMtx[QoS::MaxNum()];

// xmacro of singletons that can only fetch by Facade
#ifdef FFRT_OH_TRACE_ENABLE
#define SINGLETON_LIST \
    SINGLETON(DependenceManager) \
    SINGLETON(IOPoller) \
    SINGLETON(TimerManager) \
    SINGLETON(Scheduler) \
    SINGLETON(WorkerMonitor) \
    SINGLETON(ExecuteUnit) \
    SINGLETON(DelayedWorker) \
    SINGLETON(CoStackAttr) \
    SINGLETON(QueueMonitor) \
    SINGLETON(TraceChainAdapter) \
    SINGLETON(TraceAdapter)
#else
#define SINGLETON_LIST \
    SINGLETON(DependenceManager) \
    SINGLETON(IOPoller) \
    SINGLETON(TimerManager) \
    SINGLETON(Scheduler) \
    SINGLETON(WorkerMonitor) \
    SINGLETON(ExecuteUnit) \
    SINGLETON(DelayedWorker) \
    SINGLETON(CoStackAttr) \
    SINGLETON(QueueMonitor) \
    SINGLETON(TraceChainAdapter)
#endif

// global ptr for fast-path singleton get
#define SINGLETON(TypeName) \
    inline TypeName* g##TypeName { nullptr };
SINGLETON_LIST
#undef SINGLETON

class FFRTFacade : public NonCopyable {
public:
#define SINGLETON(TypeName) \
    static TypeName& Get##TypeName() \
    { \
        if FFRT_UNLIKELY(!g##TypeName) { \
            LazyInstance(); \
        } \
        return *g##TypeName; \
    }
SINGLETON_LIST
#undef SINGLETON

private:
    static FFRTFacade& LazyInstance();
    FFRTFacade();
};

} // namespace FFRT
#endif