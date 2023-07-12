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
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "dfx/trace/ffrt_trace.h"
#include "core/dependence_manager.h"
#include "core/entity.h"
#include "sched/scheduler.h"
#include "sync/sync.h"
#include "util/slab.h"
#ifndef _MSC_VER
#include <sys/mman.h>
#else
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#endif
#include "sched/sched_deadline.h"
#include "sync/perf_counter.h"
#include "sync/io_poller.h"
#include "dfx/bbox/bbox.h"


static thread_local CoRoutineEnv* g_CoThreadEnv = nullptr;

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
    {
        FFRT_LOGI("Execute func() task[%lu], name[%s]", co->task->gid, co->task->label.c_str());
        auto f = reinterpret_cast<ffrt_function_header_t*>(co->task->func_storage);
        auto exp = ffrt::SkipStatus::SUBMITTED;
        if (likely(__atomic_compare_exchange_n(&co->task->skipped, &exp, ffrt::SkipStatus::EXECUTED, 0,
            __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))) {
            f->exec(f);
        }
        f->destroy(f);
    }
    FFRT_TASKDONE_MARKER(co->task->gid);
    co->task->UpdateState(ffrt::TaskState::EXITED);
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
#ifndef _MSC_VER
    size_t p_size = getpagesize();
#else
    size_t p_size = 4 * 1024;
#endif
    if (CoStackAttr::Instance()->size < p_size * 3) {
        abort();
    }

#ifndef _MSC_VER
    uint64_t mp = reinterpret_cast<uint64_t>(co->stkMem.stk);
    mp = (mp + p_size - 1) / p_size * p_size;
    int ret = mprotect(reinterpret_cast<void *>(static_cast<uintptr_t>(mp)), p_size, PROT_READ);
    if (ret < 0) {
        printf("coroutine size:%lu, mp:0x%lx, page_size:%zu,result:%d,prot:%d, err:%d,%s.\n",
            static_cast<unsigned long>(sizeof(struct CoRoutine)), static_cast<unsigned long>(mp),
            p_size, ret, prot, errno, strerror(errno));
        abort();
    }
#endif
}

static inline CoRoutine* AllocNewCoRoutine(void)
{
    std::size_t stack_size = CoStackAttr::Instance()->size + sizeof(CoRoutine) - 8;
    CoRoutine* co = ffrt::QSimpleAllocator<CoRoutine>::allocMem(stack_size);
    if (co == nullptr) {
        abort();
    }

    co->stkMem.size = CoStackAttr::Instance()->size;
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
    ffrt::QSimpleAllocator<CoRoutine>::freeMem(co);
}

void CoWorkerExit()
{
    if (g_CoThreadEnv) {
        ::free(g_CoThreadEnv);
        g_CoThreadEnv = nullptr;
    }
}

static inline void BindNewCoRoutione(ffrt::TaskCtx* task)
{
    task->coRoutine = g_CoThreadEnv->runningCo;
    task->coRoutine->task = task;
    task->coRoutine->thEnv = g_CoThreadEnv;
}

static inline void UnbindCoRoutione(ffrt::TaskCtx* task)
{
    task->coRoutine->task = nullptr;
    task->coRoutine = nullptr;
}

static inline int CoAlloc(ffrt::TaskCtx* task)
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
static inline int CoCreat(ffrt::TaskCtx* task)
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
            CoStackAttr::Instance()->size - co->ctx.regs[REG_SP]);
        FFRT_LOGE("stack over flow, check local variable in you tasks or use api 'ffrt_set_co_stack_attribute'.\n");
        abort();
    }
}

static inline void CoSwitchInTrace(ffrt::TaskCtx* task)
{
    if (task->coRoutine->status == static_cast<int>(CoStatus::CO_NOT_FINISH)) {
        for (auto name : task->traceTag) {
            FFRT_TRACE_BEGIN(name.c_str());
        }
    }
    FFRT_FAKE_TRACE_MARKER(task->gid);
}

static inline void CoSwitchOutTrace(ffrt::TaskCtx* task)
{
    FFRT_FAKE_TRACE_MARKER(task->gid);
    int traceTagNum = static_cast<int>(task->traceTag.size());
    for (int i = 0; i < traceTagNum; ++i) {
        FFRT_TRACE_END();
    }
}

// called by thread work
void CoStart(ffrt::TaskCtx* task)
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

    for (;;) {
        FFRT_LOGI("Costart task[%lu], name[%s]", task->gid, task->label.c_str());
        ffrt::TaskLoadTracking::Begin(task);
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
            FFRT_LOGI("Cowait task[%lu], name[%s]", task->gid, task->label.c_str());
#ifdef FFRT_BBOX_ENABLE
            TaskSwitchCounterInc();
#endif
            return;
        }
        FFRT_WAKE_TRACER(task->gid); // fast path wk
        g_CoThreadEnv->runningCo = co;
    }
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

void CoWait(const std::function<bool(ffrt::TaskCtx*)>& pred)
{
    g_CoThreadEnv->pending = &pred;
    CoYield();
}

void CoWake(ffrt::TaskCtx* task, bool timeOut)
{
    if (task == nullptr) {
        FFRT_LOGE("task is nullptr");
        return;
    }
    // Fast path: state transition without lock
    FFRT_LOGI("Cowake task[%lu], name[%s], timeOut[%d]", task->gid, task->label.c_str(), timeOut);
    task->wakeupTimeOut = timeOut;
    FFRT_WAKE_TRACER(task->gid);
    task->UpdateState(ffrt::TaskState::READY);
}

#ifdef USE_STACKLESS_COROUTINE
static inline void ffrt_exec_callable_wrapper(void* t)
{
    ffrt::ffrt_callable_t* f=(ffrt::ffrt_callable_t*)t;
    f->exec(f->callable);
}

static inline void ffrt_destroy_callable_wrapper(void* t)
{
    ffrt::ffrt_callable_t* f=(ffrt::ffrt_callable_t*)t;
    f->destroy(f->callable);
}

static void OnStacklessCoroutineReady(ffrt::TaskCtx* task)
{
    task->lock.lock();
    task->state.SetCurState(ffrt::TaskState::State::EXITED);
    if(task->stackless_coroutine_wake_count>0){
        //log
    }
    task->lock.unlock();
    if(task->wakeFlag&&task->wake_callable_on_finish.exec){
        ffrt_exec_callable_wrapper(static_cast<void*>(&(task->wake_callable_on_finish)));
    }
    if(task->wakeFlag&&task->wake_callable_on_finish.destroy){
        ffrt_destroy_callable_wrapper(static_cast<void*>(&(task->wake_callable_on_finish)));
    }
    auto f=(ffrt_function_header_t*)task->func_storage;
    f->destroy(f);

    ffrt::DependenceManager::Instance()->onTaskDone(task);
}

void StacklessCouroutineStart(ffrt::TaskCtx* task)
{
    assert(task->coroutine_type==ffrt_coroutine_stackless);
    auto f=(ffrt_function_header_t*)task->func_storage;
    ffrt_coroutine_ptr_t coroutine=(ffrt_coroutine_ptr_t)f->exec;
    ffrt_coroutine_ret_t ret=coroutine(f);
    if(ret==ffrt_coroutine_ready){
        OnStacklessCoroutineReady(task);
    }else{
        task->lock.lock();
        task->stackless_coroutine_wake_count-=1;
        if(task->stackless_coroutine_wake_count>0){
            task->state.SetCurState(ffrt::TaskState::State::READY);
            task->lock.unlock();
            ffrt::FFRTScheduler::Instance()->PushTask(task);
        }else{
            task->state.SetCurState(ffrt::TaskState::State::BLOCKED);
            task->lock.unlock();
        }
    }
}

#ifdef __cplusplus
extern "C"{
#endif

API_ATTRIBUTE((visibility("default")))
void ffrt_wake_coroutine(void *taskin)
{
    ffrt::TaskCtx *task=(ffrt::TaskCtx *)taskin;
    std::unique_lock<decltype(task->lock)>lck(task->lock);
    if(task->state.CurState()>=ffrt::TaskState::State::EXITED){
        return;
    }
    task->stackless_coroutine_wake_count+=1;
    if(task->stackless_coroutine_wake_count==1){
        task->state.SetCurState(ffrt::TaskState::State::READY);
        task->lock.unlock();
        ffrt::FFRTScheduler::Instance()->WakeupTask(task);
    }
}
#ifdef __cplusplus
}
#endif
#endif