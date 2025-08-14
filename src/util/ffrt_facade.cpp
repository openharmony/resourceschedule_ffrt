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
#include "util/ffrt_facade.h"
#include "core/version_ctx.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/osal.h"
#include "sched/execute_ctx.h"
#include "tm/io_task.h"
#include "tm/queue_task.h"
#include "tm/uv_task.h"
#include "util/slab.h"

namespace {
constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
char g_processName[PROCESS_NAME_BUFFER_LENGTH] {};
std::atomic<bool> g_exitFlag { false };
std::atomic<bool> g_delayedWorkerExitFlag { false };
std::shared_mutex g_exitMtx;
std::once_flag g_processNameInitFlag;
}

namespace ffrt {
bool GetExitFlag()
{
    return g_exitFlag.load();
}

std::shared_mutex& GetExitMtx()
{
    return g_exitMtx;
}

const char* GetCurrentProcessName()
{
    std::call_once(g_processNameInitFlag, []() {
        GetProcessName(g_processName, PROCESS_NAME_BUFFER_LENGTH);
        if (strlen(g_processName) == 0) {
            FFRT_LOGW("Get process name failed");
        }
    });
    return g_processName;
}

bool GetDelayedWorkerExitFlag()
{
    return g_delayedWorkerExitFlag;
}

void SetDelayedWorkerExitFlag()
{
    g_delayedWorkerExitFlag.store(true);
}

class ProcessExitManager {
public:
    static ProcessExitManager& Instance()
    {
        static ProcessExitManager instance;
        return instance;
    }

    ProcessExitManager(const ProcessExitManager&) = delete;
    ProcessExitManager& operator=(const ProcessExitManager&) = delete;

private:
    ProcessExitManager() {}

    ~ProcessExitManager()
    {
        FFRT_LOGW("ProcessExitManager destruction enter");
        std::lock_guard lock(g_exitMtx);
        g_exitFlag.store(true);
    }
};

FFRTFacade& FFRTFacade::Instance()
{
    static FFRTFacade facade;
    return facade;
}
FFRTFacade::FFRTFacade()
{
// control construct sequences of singletons
#ifdef FFRT_OH_TRACE_ENABLE
    TraceAdapter::Instance();
#endif
    SimpleAllocator<QueueTask>::Instance();
    SimpleAllocator<IOTask>::Instance();
    SimpleAllocator<UVTask>::Instance();
    SimpleAllocator<VersionCtx>::Instance();
    SimpleAllocator<WaitUntilEntry>::Instance();
    TaskFactory<CPUEUTask>::Instance();
    TaskFactory<QueueTask>::Instance();
    TaskFactory<IOTask>::Instance();
    TaskFactory<UVTask>::Instance();
    DependenceManager::Instance();
    QSimpleAllocator<CoRoutine>::Instance(CoStackAttr::Instance()->size);
    CoRoutineFactory::Instance();
    TimerManager::Instance();
    Scheduler::Instance();
#ifdef FFRT_WORKER_MONITOR
    WorkerMonitor::GetInstance();
#endif
    /* By calling `FuncManager::Instance()` we force the construction
     * of FunManager singleton static object to complete before static object `SExecuteUnit` construction.
     * This implies that the destruction of `SExecuteUnit` will happen before `FuncManager`.
     * And the destructor of `SExecuteUnit` waits for all threads/CPUWorkers to finish. This way
     * we prevent use-after-free on `func_map` in `FuncManager`, when accessed by
     * `CPUWorker` objects while being destructed. Note that `CPUWorker` destruction
     * is managed by `unique_ptr` and we don't know exactly when it happens.
     */
    FuncManager::Instance();
    /* Note that by constructing `DependenceManager` before `DelayedWorker`,
     * we make sure that the lifetime of `DependenceManager` is longer than `DelayedWorker`.
     * That is necessary because `DelayedWorker` may create async tasks (CPUWorker objects) which
     * call `SDependenceManager::onTaskDone`. When that happens `SDependenceManager` must still
     * be alive. `DelayedWorker` destructor waits for all async tasks to complete first, so the
     * order will be (completion of async tasks) -> `~DelayedWorker()` -> `~SDependenceManager`.
     */
    DelayedWorker::GetInstance();
    /* Same argument as above for ExecuteUnit. ExecuteUnit destructor is what waits on all
     * threads to be done and delays destruction of main objects. It also initiates the
     * tearDown. We must avoid the situation where detached threads or timer tasks
     * are calling `SDependenceManager::onTaskDone` on destroyed SDependenceManager object.
     */
    ExecuteUnit::Instance();
    /* Ensure that IOPoller is destructed after SExecuteUnit.
     * We need to make sure that the runner in IOPoller is not
     * going to call `SExecuteUnit::WakeupWorkers`
     * or `ffrt::SExecuteUnit::PokeImpl`, while SExecuteUnit
     * is being destroyed.
     */
    IOPoller::Instance();
    ProcessExitManager::Instance();
    InitWhiteListFlag();
}
} // namespace FFRT
