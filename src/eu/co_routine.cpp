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

#include "co_routine.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/mman.h>
#include <unordered_map>
#include "ffrt_trace.h"
#include "dm/dependence_manager.h"
#include "core/entity.h"
#include "queue/serial_task.h"
#include "sched/scheduler.h"
#include "sync/sync.h"
#include "util/slab.h"
#include "sched/sched_deadline.h"
#include "sync/perf_counter.h"
#include "sync/io_poller.h"
#include "dfx/bbox/bbox.h"
#include "co_routine_factory.h"
#include "pthread_ffrt.h"

#ifdef ASYNC_STACKTRACE
#include "dfx/async_stack/ffrt_async_stack.h"
#endif

using namespace ffrt;
static pthread_key_t g_coThreadTlsKey = 0;
static pthread_once_t g_coThreadTlsKeyOnce = PTHREAD_ONCE_INIT;

using namespace OHOS::HiviewDFX;

extern pthread_key_t g_executeCtxTlsKey;

static void CoEnvDestructor(void* args)
{
    auto coEnv = static_cast<CoRoutineEnv*>(args);
    if (coEnv) {
        delete coEnv;
    }
}

static void MakeCoEnvTlsKey()
{
    pthread_key_create(&g_coThreadTlsKey, CoEnvDestructor);
}

static CoRoutineEnv* GetCoEnv()
{
    CoRoutineEnv* coEnv = nullptr;
    pthread_once(&g_coThreadTlsKeyOnce, MakeCoEnvTlsKey);

    void *curTls = pthread_getspecific(g_coThreadTlsKey);
    if (curTls != nullptr) {
        coEnv = reinterpret_cast<CoRoutineEnv *>(curTls);
    } else {
        coEnv = new (std::nothrow) CoRoutineEnv();
        pthread_setspecific(g_coThreadTlsKey, coEnv);
    }
    return coEnv;
}

#ifdef FFRT_TASK_LOCAL_ENABLE
namespace {
bool IsTaskLocalEnable(ffrt::CPUEUTask* task)
{
    if (task->type != ffrt_normal_task) {
        return false;
    }
    if (!task->taskLocal) {
        return false;
    }
    if (task->tsd == nullptr) {
        FFRT_LOGE("taskLocal enabled but task tsd invalid");
        abort();
    }

    return true;
}

void InitWorkerTsdValueToTask(void** taskTsd)
{
    std::unordered_map<std::string, pthread_key_t> updKeyMap = {
        {"g_executeCtxTlsKey", g_executeCtxTlsKey},
        {"g_coThreadTlsKey", g_coThreadTlsKey}
    };
    auto threadTsd = pthread_gettsd();
    for (const auto& updPair : updKeyMap) {
        pthread_key_t key = updPair.second;
        auto keyName = updPair.first;
        if (key <= 0) {
            FFRT_LOGE("[%s] key[%d] invalid", keyName.c_str(), key);
            abort();
        }

        auto addr = threadTsd[key];
        if (addr) {
            taskTsd[key] = addr;
            FFRT_LOGI("copy [%s] key=[%d] addr=[%llx] from thread to task", keyName.c_str(), key, (uint64_t)addr);
        } else if (keyName == "g_executeCtxTlsKey") {
            FFRT_LOGE("g_executeCtxTlsKey addr invalid");
            abort();
        }
    }
}

void SwitchTsdAddrToTask(ffrt::CPUEUTask* task)
{
    auto threadTsd = pthread_gettsd();
    task->threadTsd = threadTsd;
    pthread_settsd(task->tsd);
}
} // namespace

static void SwitchTsdToTask(ffrt::CPUEUTask* task)
{
    if (!IsTaskLocalEnable(task)) {
        return;
    }

    InitWorkerTsdValueToTask(task->tsd);

    SwitchTsdAddrToTask(task);
    FFRT_LOGI("switch tsd to task Success");
}

namespace {
bool SwitchTsdAddrToThread(ffrt::CPUEUTask* task)
{
    if (!task->threadTsd) {
        return false;
    }
    pthread_settsd(task->threadTsd);
    FFRT_LOGI("switch to backend thread tsd[%llx]", (uint64_t)(task->threadTsd));
    task->threadTsd = nullptr;
    return true;
}

void UpdateWorkerTsdValueToThread(void** taskTsd)
{
    std::unordered_map<std::string, pthread_key_t> updKeyMap = {
        {"g_executeCtxTlsKey", g_executeCtxTlsKey},
        {"g_coThreadTlsKey", g_coThreadTlsKey}
    };
    auto threadTsd = pthread_gettsd();
    for (const auto& updPair : updKeyMap) {
        pthread_key_t key = updPair.second;
        auto keyName = updPair.first;
        if (key <= 0) {
            FFRT_LOGE("[%s] key[%d] invalid", keyName.c_str(), key);
            abort();
        }

        auto threadVal = threadTsd[key];
        auto taskVal = taskTsd[key];
        if (!threadVal && taskVal) {
            threadTsd[key] = taskVal;
            FFRT_LOGW("new [%s] key=[%d] addr=[%llx] from task to thread", keyName.c_str(), key, (uint64_t)threadVal);
        } else if (threadVal && taskVal) {
            if (threadVal != taskVal) {
                FFRT_LOGE("mismatch [%s] key=[%d] thread addr=[%llx], task addr=[%llx]",
                    keyName.c_str(), key, (uint64_t)threadVal, (uint64_t)taskVal);
                abort();
            }
        } else if (threadVal && !taskVal) {
            FFRT_LOGE("unexpected: thread exists but task not exists");
            abort();
        }
        taskTsd[key] = nullptr;
    }
}
} // namespace

static void SwitchTsdToThread(ffrt::CPUEUTask* task)
{
    if (!IsTaskLocalEnable(task)) {
        return;
    }

    if (!SwitchTsdAddrToThread(task)) {
        return;
    }

    UpdateWorkerTsdValueToThread(task->tsd);
    FFRT_LOGI("switch tsd to thread Success");
}

namespace {
void TaskTsdRunDtors(ffrt::CPUEUTask* task)
{
    SwitchTsdAddrToTask(task);
    pthread_tsd_run_dtors();
    SwitchTsdAddrToThread(task);
}
} // namespace

void TaskTsdDeconstruct(ffrt::CPUEUTask* task)
{
    if (!IsTaskLocalEnable(task)) {
        return;
    }

    if (task->threadTsd != nullptr) {
        FFRT_LOGE("thread tsd[%llx] not null", (uint64_t)task->threadTsd);
    }

    TaskTsdRunDtors(task);
    if (task->tsd != nullptr) {
        FFRT_LOGI("clear task tsd[%llx]", (uint64_t)(task->tsd));
        free(task->tsd);
        task->tsd = nullptr;
        task->taskLocal = false;
    }
    FFRT_LOGI("task tsd deconstruct done");
}
#endif

static inline void CoSwitch(CoCtx* from, CoCtx* to)
{
    if (co2_save_context(from) == 0) {
        co2_restore_context(to);
    }
}

static inline void CoExit(CoRoutine* co, bool isNormalTask)
{
#ifdef FFRT_TASK_LOCAL_ENABLE
    if (isNormalTask) {
        SwitchTsdToThread(co->task);
    }
#endif
    CoSwitch(&co->ctx, &co->thEnv->schCtx);
}

static inline void CoStartEntry(void* arg)
{
    CoRoutine* co = reinterpret_cast<CoRoutine*>(arg);
    ffrt::CPUEUTask* task = co->task;
    bool isNormalTask = false;
    switch (task->type) {
        case ffrt_normal_task: {
            isNormalTask = true;
            task->Execute();
            break;
        }
        case ffrt_serial_task: {
            SerialTask* sTask = reinterpret_cast<SerialTask*>(task);
            // Before the batch execution is complete, head node cannot be released.
            sTask->IncDeleteRef();
            sTask->Execute();
            sTask->DecDeleteRef();
            break;
        }
        default: {
            FFRT_LOGE("CoStart unsupport task[%lu], type=%d, name[%s]", task->gid, task->type, task->label.c_str());
            break;
        }
    }

    co->status.store(static_cast<int>(CoStatus::CO_UNINITIALIZED));
    CoExit(co, isNormalTask);
}

static void CoSetStackProt(CoRoutine* co, int prot)
{
    /* set the attribute of the page table closest to the stack top in the user stack to read-only,
     * and 1~2 page table space will be wasted
     */
    size_t p_size = getpagesize();
    if (co->stkMem.size < p_size * 3) {
        abort();
    }

    uint64_t mp = reinterpret_cast<uint64_t>(co->stkMem.stk);
    mp = (mp + p_size - 1) / p_size * p_size;
    int ret = mprotect(reinterpret_cast<void *>(static_cast<uintptr_t>(mp)), p_size, prot);
    if (ret < 0) {
        printf("coroutine size:%lu, mp:0x%lx, page_size:%zu,result:%d,prot:%d, err:%d,%s.\n",
            static_cast<unsigned long>(sizeof(struct CoRoutine)), static_cast<unsigned long>(mp),
            p_size, ret, prot, errno, strerror(errno));
        abort();
    }
}

static inline CoRoutine* AllocNewCoRoutine(void)
{
    std::size_t stack_size = CoStackAttr::Instance()->size;
    CoRoutine* co = ffrt::CoRoutineAllocMem(stack_size);
    if (co == nullptr) {
        abort();
    }

    co->stkMem.size = static_cast<uint64_t>(CoStackAttr::Instance()->size - sizeof(CoRoutine) + 8);
    co->stkMem.magic = STACK_MAGIC;
    if (CoStackAttr::Instance()->type == CoStackProtectType::CO_STACK_STRONG_PROTECT) {
        CoSetStackProt(co, PROT_READ);
    }
    co->status.store(static_cast<int>(CoStatus::CO_UNINITIALIZED));
    return co;
}

static inline void CoMemFree(CoRoutine* co)
{
    if (CoStackAttr::Instance()->type == CoStackProtectType::CO_STACK_STRONG_PROTECT) {
        CoSetStackProt(co, PROT_WRITE | PROT_READ);
    }
    ffrt::CoRoutineFreeMem(co);
}

void CoStackFree(void)
{
    if (GetCoEnv()) {
        if (GetCoEnv()->runningCo) {
            CoMemFree(GetCoEnv()->runningCo);
            GetCoEnv()->runningCo = nullptr;
        }
    }
}

void CoWorkerExit(void)
{
    CoStackFree();
}

static inline void BindNewCoRoutione(ffrt::CPUEUTask* task)
{
    task->coRoutine = GetCoEnv()->runningCo;
    task->coRoutine->task = task;
    task->coRoutine->thEnv = GetCoEnv();
}

static inline void UnbindCoRoutione(ffrt::CPUEUTask* task)
{
    task->coRoutine->task = nullptr;
    task->coRoutine = nullptr;
}

static inline int CoAlloc(ffrt::CPUEUTask* task)
{
    if (task->coRoutine) {
        if (GetCoEnv()->runningCo) {
            CoMemFree(GetCoEnv()->runningCo);
        }
        GetCoEnv()->runningCo = task->coRoutine;
    } else {
        if (!GetCoEnv()->runningCo) {
            GetCoEnv()->runningCo = AllocNewCoRoutine();
        }
    }
    BindNewCoRoutione(task);
    return 0;
}

// call CoCreat when task creat
static inline int CoCreat(ffrt::CPUEUTask* task)
{
    CoAlloc(task);
    auto co = task->coRoutine;
    if (co->status.load() == static_cast<int>(CoStatus::CO_UNINITIALIZED)) {
        co2_init_context(&co->ctx, CoStartEntry, static_cast<void*>(co), co->stkMem.stk, co->stkMem.size);
    }
    return 0;
}

static inline void CoStackCheck(CoRoutine* co)
{
    if (co->stkMem.magic != STACK_MAGIC) {
        FFRT_LOGE("sp offset:%lu.\n", (uint64_t)co->stkMem.stk +
            co->stkMem.size - co->ctx.regs[FFRT_REG_SP]);
        FFRT_LOGE("stack over flow, check local variable in you tasks or use api 'ffrt_set_co_stack_attribute'.\n");
        abort();
    }
}

static inline void CoSwitchInTrace(ffrt::CPUEUTask* task)
{
    if (task->coRoutine->status == static_cast<int>(CoStatus::CO_NOT_FINISH)) {
        for (auto name : task->traceTag) {
            FFRT_TRACE_BEGIN(name.c_str());
        }
    }
    FFRT_FAKE_TRACE_MARKER(task->gid);
}

static inline void CoSwitchOutTrace(ffrt::CPUEUTask* task)
{
    FFRT_FAKE_TRACE_MARKER(task->gid);
    int traceTagNum = static_cast<int>(task->traceTag.size());
    for (int i = 0; i < traceTagNum; ++i) {
        FFRT_TRACE_END();
    }
}

// called by thread work
void CoStart(ffrt::CPUEUTask* task)
{
    if (task->coRoutine) {
        int ret = task->coRoutine->status.exchange(static_cast<int>(CoStatus::CO_RUNNING));
        if (ret == static_cast<int>(CoStatus::CO_RUNNING) && GetBboxEnableState() != 0) {
            FFRT_LOGE("executed by worker suddenly, ignore backtrace");
            return;
        }
    }
    CoCreat(task);
    auto co = task->coRoutine;

#ifdef FFRT_BBOX_ENABLE
    TaskRunCounterInc();
#endif

#ifdef FFRT_HITRACE_ENABLE
    using namespace OHOS::HiviewDFX;
    HiTraceId currentId = HiTraceChain::GetId();
    if (task != nullptr && task->traceId_ != nullptr) {
        HiTraceChain::SaveAndSet(*(task->traceId_));
        HiTraceChain::Tracepoint(HITRACE_TP_SR, *(task->traceId_), "ffrt::CoStart");
    }
#endif

    for (;;) {
        FFRT_LOGD("Costart task[%lu], name[%s]", task->gid, task->label.c_str());
        ffrt::TaskLoadTracking::Begin(task);
#ifdef ASYNC_STACKTRACE
        FFRTSetStackId(task->stackId);
#endif
        FFRT_TASK_BEGIN(task->label, task->gid);
        CoSwitchInTrace(task);
#ifdef FFRT_TASK_LOCAL_ENABLE
        SwitchTsdToTask(co->task);
#endif
        CoSwitch(&co->thEnv->schCtx, &co->ctx);
        if (co->isTaskDone) {
            task->UpdateState(ffrt::TaskState::EXITED);
            co->isTaskDone = false;
        }
        FFRT_TASK_END();
        ffrt::TaskLoadTracking::End(task); // Todo: deal with CoWait()
        CoStackCheck(co);
        auto pending = GetCoEnv()->pending;
        if (pending == nullptr) {
#ifdef FFRT_BBOX_ENABLE
            TaskFinishCounterInc();
#endif
            break;
        }
        GetCoEnv()->pending = nullptr;
        // Fast path: skip state transition
        if ((*pending)(task)) {
            FFRT_LOGD("Cowait task[%lu], name[%s]", task->gid, task->label.c_str());
#ifdef FFRT_BBOX_ENABLE
            TaskSwitchCounterInc();
#endif
#ifdef FFRT_HITRACE_ENABLE
        HiTraceChain::Tracepoint(HITRACE_TP_SS, HiTraceChain::GetId(), "ffrt::CoStart");
        HiTraceChain::Restore(currentId);
#endif
            return;
        }
        FFRT_WAKE_TRACER(task->gid); // fast path wk
        GetCoEnv()->runningCo = co;
    }
#ifdef FFRT_HITRACE_ENABLE
    HiTraceChain::Tracepoint(HITRACE_TP_SS, HiTraceChain::GetId(), "ffrt::CoStart");
    HiTraceChain::Restore(currentId);
#endif
}

// called by thread work
void CoYield(void)
{
    CoRoutine* co = static_cast<CoRoutine*>(GetCoEnv()->runningCo);
    co->status.store(static_cast<int>(CoStatus::CO_NOT_FINISH));
    GetCoEnv()->runningCo = nullptr;
    CoSwitchOutTrace(co->task);
    FFRT_BLOCK_MARKER(co->task->gid);
#ifdef FFRT_TASK_LOCAL_ENABLE
    SwitchTsdToThread(co->task);
#endif
    CoSwitch(&co->ctx, &GetCoEnv()->schCtx);
    while (GetBboxEnableState() != 0) {
        if (GetBboxEnableState() != gettid()) {
            BboxFreeze(); // freeze non-crash thread
            return;
        }
        const int IGNORE_DEPTH = 3;
        backtrace(IGNORE_DEPTH);
        co->status.store(static_cast<int>(CoStatus::CO_NOT_FINISH)); // recovery to old state
        bool isNormalTask = (co->task->type == ffrt_normal_task);
        CoExit(co, isNormalTask);
    }
}

void CoWait(const std::function<bool(ffrt::CPUEUTask*)>& pred)
{
    GetCoEnv()->pending = &pred;
    CoYield();
}

void CoWake(ffrt::CPUEUTask* task, bool timeOut)
{
    if (task == nullptr) {
        FFRT_LOGE("task is nullptr");
        return;
    }
    // Fast path: state transition without lock
    FFRT_LOGD("Cowake task[%lu], name[%s], timeOut[%d]", task->gid, task->label.c_str(), timeOut);
    task->wakeupTimeOut = timeOut;
    FFRT_WAKE_TRACER(task->gid);
    switch (task->type) {
        case ffrt_normal_task: {
            task->UpdateState(ffrt::TaskState::READY);
            break;
        }
        case ffrt_serial_task: {
            SerialTask* sTask = reinterpret_cast<SerialTask*>(task);
            auto handle = sTask->GetHandler();
            handle->TransferTask(sTask);
            break;
        }
        default: {
            FFRT_LOGE("CoWake unsupport task[%lu], type=%d, name[%s]", task->gid, task->type, task->label.c_str());
            break;
        }
    }
}

CoRoutineFactory &CoRoutineFactory::Instance()
{
    static CoRoutineFactory fac;
    return fac;
}