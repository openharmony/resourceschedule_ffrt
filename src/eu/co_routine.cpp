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
#include "queue/serial_task.h"
#include "sched/scheduler.h"
#include "sync/sync.h"
#include "util/slab.h"
#include "sched/sched_deadline.h"
#include "sync/perf_counter.h"
#include "sync/io_poller.h"
#include "dfx/bbox/bbox.h"
#include "co_routine_factory.h"

#ifdef ASYNC_STACKTRACE
#include "async_stack.h"
#endif

using namespace ffrt;
static thread_local CoRoutineEnv* g_CoThreadEnv = nullptr;

using namespace OHOS::HiviewDFX;

static inline void CoSwitch(CoCtx* from, CoCtx* to)
{
    co2_switch_context(from, to);
}

static inline void CoExit(CoRoutine* co)
{
    CoSwitch(&co->ctx, &co->thEnv->schCtx);
}

static inline void CoStartEntry(void* arg)
{
    CoRoutine* co = reinterpret_cast<CoRoutine*>(arg);
    ffrt::CPUEUTask* task = co->task;
    switch (task->type) {
        case ffrt_normal_task: {
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
    CoExit(co);
}

static inline void CoInitThreadEnv(void)
{
    g_CoThreadEnv = reinterpret_cast<CoRoutineEnv*>(calloc(1, sizeof(CoRoutineEnv)));
    CoRoutineEnv* t = g_CoThreadEnv;
    if (!t) {
        abort();
    }
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
    if (g_CoThreadEnv) {
        if (g_CoThreadEnv->runningCo) {
            CoMemFree(g_CoThreadEnv->runningCo);
            g_CoThreadEnv->runningCo = nullptr;
        }
    }
}

void CoWorkerExit(void)
{
    if (g_CoThreadEnv) {
        if (g_CoThreadEnv->runningCo) {
            CoMemFree(g_CoThreadEnv->runningCo);
            g_CoThreadEnv->runningCo = nullptr;
        }
        ::free(g_CoThreadEnv);
        g_CoThreadEnv = nullptr;
    }
}

static inline void BindNewCoRoutione(ffrt::CPUEUTask* task)
{
    task->coRoutine = g_CoThreadEnv->runningCo;
    task->coRoutine->task = task;
    task->coRoutine->thEnv = g_CoThreadEnv;
}

static inline void UnbindCoRoutione(ffrt::CPUEUTask* task)
{
    task->coRoutine->task = nullptr;
    task->coRoutine = nullptr;
}

static inline int CoAlloc(ffrt::CPUEUTask* task)
{
    if (!g_CoThreadEnv) {
        CoInitThreadEnv();
    }
    if (task->coRoutine) {
        if (g_CoThreadEnv->runningCo) {
            CoMemFree(g_CoThreadEnv->runningCo);
        }
        g_CoThreadEnv->runningCo = task->coRoutine;
    } else {
        if (!g_CoThreadEnv->runningCo) {
            g_CoThreadEnv->runningCo = AllocNewCoRoutine();
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
            FFRT_BBOX_LOG("executed by worker suddenly, ignore backtrace");
            return;
        }
    }
    CoCreat(task);
    auto co = task->coRoutine;

#ifdef FFRT_BBOX_ENABLE
    TaskRunCounterInc();
#endif

#ifdef ENABLE_HITRACE
        using namespace OHOS::HiviewDFX;
        auto scpuTask = reinterpret_cast<ffrt::SCPUEUTask*>(task);
        HiTraceId currentId = HiTraceChain::GetId();
        if (scpuTask->traceId_ != nullptr) {
            HiTraceChain::SaveAndSet(*(scpuTask->traceId_));
            HiTraceChain::Tracepoint(HITRACE_TP_SR, *(scpuTask->traceId_), "ffrt::CoStart");
        }
#endif

    for (;;) {
        FFRT_LOGD("Costart task[%lu], name[%s]", task->gid, task->label.c_str());
        ffrt::TaskLoadTracking::Begin(task);
#ifdef ASYNC_STACKTRACE
        SetStackId(task->stackId);
#endif
        FFRT_TASK_BEGIN(task->label, task->gid);
        CoSwitchInTrace(task);

        CoSwitch(&co->thEnv->schCtx, &co->ctx);
        FFRT_TASK_END();
        ffrt::TaskLoadTracking::End(task); // Todo: deal with CoWait()
        CoStackCheck(co);
        auto pending = g_CoThreadEnv->pending;
        if (pending == nullptr) {
#ifdef FFRT_BBOX_ENABLE
            TaskFinishCounterInc();
#endif
            break;
        }
        g_CoThreadEnv->pending = nullptr;
        // Fast path: skip state transition
        if ((*pending)(task)) {
            FFRT_LOGD("Cowait task[%lu], name[%s]", task->gid, task->label.c_str());
#ifdef FFRT_BBOX_ENABLE
            TaskSwitchCounterInc();
#endif
#ifdef ENABLE_HITRACE
        HiTraceChain::Tracepoint(HITRACE_TP_SS, HiTraceChain::GetId(), "ffrt::CoStart");
        HiTraceChain::Restore(currentId);
#endif
            return;
        }
        FFRT_WAKE_TRACER(task->gid); // fast path wk
        g_CoThreadEnv->runningCo = co;
    }
#ifdef ENABLE_HITRACE
    HiTraceChain::Tracepoint(HITRACE_TP_SS, HiTraceChain::GetId(), "ffrt::CoStart");
    HiTraceChain::Restore(currentId);
#endif
}

// called by thread work
void CoYield(void)
{
    CoRoutine* co = static_cast<CoRoutine*>(g_CoThreadEnv->runningCo);
    co->status.store(static_cast<int>(CoStatus::CO_NOT_FINISH));
    g_CoThreadEnv->runningCo = nullptr;
    CoSwitchOutTrace(co->task);
    FFRT_BLOCK_MARKER(co->task->gid);
    CoSwitch(&co->ctx, &g_CoThreadEnv->schCtx);
    while (GetBboxEnableState() != 0) {
        if (GetBboxEnableState() != gettid()) {
            BboxFreeze(); // freeze non-crash thread
            return;
        }
        const int IGNORE_DEPTH = 3;
        backtrace(IGNORE_DEPTH);
        co->status.store(static_cast<int>(CoStatus::CO_NOT_FINISH)); // recovery to old state
        CoExit(co);
    }
}

void CoWait(const std::function<bool(ffrt::CPUEUTask*)>& pred)
{
    g_CoThreadEnv->pending = &pred;
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
