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
#include "ffrt_trace.h"
#include "dm/dependence_manager.h"
#include "core/entity.h"
#include "tm/queue_task.h"
#include "sched/scheduler.h"
#include "sync/sync.h"
#include "util/slab.h"
#include "sched/sched_deadline.h"
#include "sync/perf_counter.h"
#include "sync/io_poller.h"
#include "dfx/bbox/bbox.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "co_routine_factory.h"
#ifdef FFRT_TASK_LOCAL_ENABLE
#include "pthread_ffrt.h"
#endif
#ifdef FFRT_ASYNC_STACKTRACE
#include "dfx/async_stack/ffrt_async_stack.h"
#ifdef FFRT_TASK_LOCAL_ENABLE
#include "pthread_ffrt.h"
#endif
#endif

using namespace ffrt;

static inline void CoStackCheck(CoRoutine* co)
{
    if (unlikely(co->stkMem.magic != STACK_MAGIC)) {
        FFRT_LOGE("sp offset:%p.\n", co->stkMem.stk +
            co->stkMem.size - co->ctx.regs[REG_SP]);
        FFRT_LOGE("stack over flow, check local variable in you tasks or use api 'ffrt_task_attr_set_stack_size'.\n");
    }
}

extern pthread_key_t g_executeCtxTlsKey;
pthread_key_t g_coThreadTlsKey = 0;
pthread_once_t g_coThreadTlsKeyOnce = PTHREAD_ONCE_INIT;
void CoEnvDestructor(void* args)
{
    auto coEnv = static_cast<CoRoutineEnv*>(args);
    if (coEnv) {
        delete coEnv;
    }
}

void MakeCoEnvTlsKey()
{
    pthread_key_create(&g_coThreadTlsKey, CoEnvDestructor);
}

CoRoutineEnv* GetCoEnv()
{
    CoRoutineEnv* coEnv = nullptr;
    pthread_once(&g_coThreadTlsKeyOnce, MakeCoEnvTlsKey);

    void *curTls = pthread_getspecific(g_coThreadTlsKey);
    if (curTls != nullptr) {
        coEnv = reinterpret_cast<CoRoutineEnv *>(curTls);
    } else {
        coEnv = new CoRoutineEnv();
        pthread_setspecific(g_coThreadTlsKey, coEnv);
    }
    return coEnv;
}

#ifdef FFRT_TASK_LOCAL_ENABLE
namespace {
bool IsTaskLocalEnable(ffrt::CPUEUTask* task)
{
    if ((task->type != ffrt_normal_task) || (!task->taskLocal)) {
        return false;
    }

    if (task->tsd == nullptr) {
        FFRT_LOGE("taskLocal enabled but task tsd invalid");
        return false;
    }

    return true;
}

void InitWorkerTsdValueToTask(void** taskTsd)
{
    const pthread_key_t updKeyMap[] = {g_executeCtxTlsKey, g_coThreadTlsKey};
    auto threadTsd = pthread_gettsd();
    for (const auto& key : updKeyMap) {
        FFRT_UNLIKELY_COND_DO_ABORT(key <= 0, "key[%d] invalid", key);
        auto addr = threadTsd[key];
        if (addr) {
            taskTsd[key] = addr;
        }
    }
}

void SwitchTsdAddrToTask(ffrt::CPUEUTask* task)
{
    auto threadTsd = pthread_gettsd();
    task->threadTsd = threadTsd;
    pthread_settsd(task->tsd);
}

void SwitchTsdToTask(ffrt::CPUEUTask* task)
{
    if (!IsTaskLocalEnable(task)) {
        return;
    }

    InitWorkerTsdValueToTask(task->tsd);

    SwitchTsdAddrToTask(task);

    task->runningTid.store(pthread_self());
    FFRT_LOGD("switch tsd to task Success");
}

bool SwitchTsdAddrToThread(ffrt::CPUEUTask* task)
{
    if (!task->threadTsd) {
        return false;
    }
    pthread_settsd(task->threadTsd);
    task->threadTsd = nullptr;
    return true;
}

void UpdateWorkerTsdValueToThread(void** taskTsd)
{
    const pthread_key_t updKeyMap[] = {g_executeCtxTlsKey, g_coThreadTlsKey};
    auto threadTsd = pthread_gettsd();
    for (const auto& key : updKeyMap) {
        FFRT_UNLIKELY_COND_DO_ABORT(key <= 0, "key[%d] invalid", key);
        auto threadVal = threadTsd[key];
        auto taskVal = taskTsd[key];
        if (!threadVal && taskVal) {
            threadTsd[key] = taskVal;
        } else {
            FFRT_UNLIKELY_COND_DO_ABORT((threadVal && taskVal && (threadVal != taskVal)), "mismatch key=[%d]", key);
            FFRT_UNLIKELY_COND_DO_ABORT((threadVal && !taskVal),
                                        "unexpected: thread exists but task not exists, key = [%d]", key);
        }
        taskTsd[key] = nullptr;
    }
}

void SwitchTsdToThread(ffrt::CPUEUTask* task)
{
    if (!IsTaskLocalEnable(task)) {
        return;
    }

    if (!SwitchTsdAddrToThread(task)) {
        return;
    }

    UpdateWorkerTsdValueToThread(task->tsd);

    task->runningTid.store(0);
    FFRT_LOGD("switch tsd to thread Success");
}

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

    TaskTsdRunDtors(task);
    if (task->tsd != nullptr) {
        free(task->tsd);
        task->tsd = nullptr;
        task->taskLocal = false;
    }
    FFRT_LOGD("tsd deconstruct done, task[%lu], name[%s]", task->gid, task->label.c_str());
}
#endif

static inline void CoSwitch(CoCtx* from, CoCtx* to)
{
    co2_switch_context(from, to);
}

static inline void CoExit(CoRoutine* co, bool isNormalTask)
{
#ifdef FFRT_TASK_LOCAL_ENABLE
    if (isNormalTask) {
        SwitchTsdToThread(co->task);
    }
#endif
    CoStackCheck(co);
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
        case ffrt_queue_task: {
            QueueTask* sTask = reinterpret_cast<QueueTask*>(task);
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
    uint64_t mp = reinterpret_cast<uint64_t>(co->stkMem.stk);
    mp = (mp + p_size - 1) / p_size * p_size;
    int ret = mprotect(reinterpret_cast<void *>(static_cast<uintptr_t>(mp)), p_size, prot);
    FFRT_UNLIKELY_COND_DO_ABORT(ret < 0, "coroutine size:%lu, mp:0x%lx, page_size:%zu, result:%d, prot:%d, err:%d, %s",
        static_cast<unsigned long>(sizeof(struct CoRoutine)), static_cast<unsigned long>(mp),
        p_size, ret, prot, errno, strerror(errno));
}

static inline CoRoutine* AllocNewCoRoutine(size_t stackSize)
{
    std::size_t defaultStackSize = CoStackAttr::Instance()->size;
    CoRoutine* co = nullptr;
    if (likely(stackSize == defaultStackSize)) {
        co = ffrt::CoRoutineAllocMem(stackSize);
    } else {
        co = static_cast<CoRoutine*>(mmap(nullptr, stackSize,
            PROT_READ | PROT_WRITE,  MAP_ANONYMOUS | MAP_PRIVATE, -1, 0));
        if (co == reinterpret_cast<CoRoutine*>(MAP_FAILED)) {
            FFRT_LOGE("memory mmap failed.");
            return nullptr;
        }
    }
    if (!co) {
        FFRT_LOGE("memory not enough");
        return nullptr;
    }
    co->allocatedSize = stackSize;
    co->stkMem.size = static_cast<uint64_t>(stackSize - sizeof(CoRoutine) + 8);
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
    std::size_t defaultStackSize = CoStackAttr::Instance()->size;
    if (likely(co->allocatedSize == defaultStackSize)) {
        ffrt::CoRoutineFreeMem(co);
    } else {
        int ret = munmap(co, co->allocatedSize);
        if (ret != 0) {
            FFRT_LOGE("munmap failed with errno: %d", errno);
        }
    }
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
    if (task->coRoutine) { // use allocated coroutine stack
        if (GetCoEnv()->runningCo) { // free cached stack if it exist
            CoMemFree(GetCoEnv()->runningCo);
        }
        GetCoEnv()->runningCo = task->coRoutine;
    } else {
        if (!GetCoEnv()->runningCo) { // if no cached stack, alloc one
            GetCoEnv()->runningCo = AllocNewCoRoutine(task->stack_size);
        } else { // exist cached stack
            if (GetCoEnv()->runningCo->allocatedSize != task->stack_size) { // stack size not match, alloc one
                CoMemFree(GetCoEnv()->runningCo); // free cached stack
                GetCoEnv()->runningCo = AllocNewCoRoutine(task->stack_size);
            }
        }
    }
    return 0;
}

// call CoCreat when task creat
static inline int CoCreat(ffrt::CPUEUTask* task)
{
    CoAlloc(task);
    BindNewCoRoutione(task);
    auto co = task->coRoutine;
    if (co->status.load() == static_cast<int>(CoStatus::CO_UNINITIALIZED)) {
        co2_init_context(&co->ctx, CoStartEntry, static_cast<void*>(co), co->stkMem.stk, co->stkMem.size);
    }
    return 0;
}

static inline void CoSwitchInTransaction(ffrt::CPUEUTask* task)
{
    if (task->coRoutine->status == static_cast<int>(CoStatus::CO_NOT_FINISH)) {
        for (auto& name : task->traceTag) {
            FFRT_TRACE_BEGIN(name.c_str());
        }
    }
    FFRT_FAKE_TRACE_MARKER(task->gid);
}

static inline void CoSwitchOutTransaction(ffrt::CPUEUTask* task)
{
    FFRT_FAKE_TRACE_MARKER(task->gid);
    int traceTagNum = static_cast<int>(task->traceTag.size());
    for (int i = 0; i < traceTagNum; ++i) {
        FFRT_TRACE_END();
    }
}

// called by thread work
void CoStart(ffrt::CPUEUTask* task, CoRoutineEnv* coRoutineEnv)
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

    FFRTTraceRecord::TaskRun(task->GetQos(), task);

    for (;;) {
        ffrt::TaskLoadTracking::Begin(task);
#ifdef FFRT_ASYNC_STACKTRACE
        FFRTSetStackId(task->stackId);
#endif
        FFRT_TASK_BEGIN(task->label, task->gid);
        if (task->type == ffrt_normal_task) {
            task->UpdateState(ffrt::TaskState::RUNNING);
        }
        CoSwitchInTransaction(task);
#ifdef FFRT_TASK_LOCAL_ENABLE
        SwitchTsdToTask(co->task);
#endif
        CoSwitch(&co->thEnv->schCtx, &co->ctx);
        FFRT_TASK_END();
        ffrt::TaskLoadTracking::End(task); // Todo: deal with CoWait()
        CoStackCheck(co);

        // 1. coroutine task done, exit normally, need to exec next coroutine task
        if (co->isTaskDone) {
            task->UpdateState(ffrt::TaskState::EXITED);
            co->isTaskDone = false;
            return;
        }

        // 2. couroutine task block, switch to thread
        // need suspend the coroutine task or continue to execute the coroutine task.
        auto pending = coRoutineEnv->pending;
        if (pending == nullptr) {
            return;
        }
        coRoutineEnv->pending = nullptr;
        FFRTTraceRecord::TaskCoSwitchOut(task);
        // Fast path: skip state transition
        if ((*pending)(task)) {
            // The ownership of the task belongs to other host(cv/mutex/epoll etc)
            // And the task cannot be accessed any more.
            return;
        }
        FFRT_WAKE_TRACER(task->gid); // fast path wk
        coRoutineEnv->runningCo = co;
    }
}

// called by thread work
void CoYield(void)
{
    CoRoutine* co = static_cast<CoRoutine*>(GetCoEnv()->runningCo);
    co->status.store(static_cast<int>(CoStatus::CO_NOT_FINISH));
    GetCoEnv()->runningCo = nullptr;
    CoSwitchOutTransaction(co->task);
    if (co->task->type == ffrt_normal_task) {
        co->task->UpdateState(ffrt::TaskState::BLOCKED);
    }
    FFRT_BLOCK_MARKER(co->task->gid);
#ifdef FFRT_TASK_LOCAL_ENABLE
    SwitchTsdToThread(co->task);
#endif
    CoStackCheck(co);
    CoSwitch(&co->ctx, &GetCoEnv()->schCtx);
    while (GetBboxEnableState() != 0) {
        if (GetBboxEnableState() != gettid()) {
            BboxFreeze(); // freeze non-crash thread
            return;
        }
        const int IGNORE_DEPTH = 3;
        backtrace(IGNORE_DEPTH);
        co->status.store(static_cast<int>(CoStatus::CO_NOT_FINISH)); // recovery to old state
        CoExit(co, co->task->type == ffrt_normal_task);
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
    task->wakeupTimeOut = timeOut;
    FFRT_WAKE_TRACER(task->gid);
    switch (task->type) {
        case ffrt_normal_task: {
            task->UpdateState(ffrt::TaskState::READY);
            break;
        }
        case ffrt_queue_task: {
            QueueTask* sTask = reinterpret_cast<QueueTask*>(task);
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
