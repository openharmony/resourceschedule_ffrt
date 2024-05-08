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

#include "sdependence_manager.h"
#include "util/worker_monitor.h"

#ifdef ASYNC_STACKTRACE
#include "dfx/async_stack/ffrt_async_stack.h"
#endif
using namespace OHOS::HiviewDFX;

namespace ffrt {

SDependenceManager::SDependenceManager() : criticalMutex_(Entity::Instance()->criticalMutex_)
{
#ifdef FFRT_OH_TRACE_ENABLE
    TraceAdapter::Instance();
#endif
    // control construct sequences of singletons
    SimpleAllocator<CPUEUTask>::instance();
    SimpleAllocator<VersionCtx>::instance();
#ifdef FFRT_IO_TASK_SCHEDULER
    PollerProxy::Instance();
#endif
    FFRTScheduler::Instance();
    ExecuteUnit::Instance();
    TaskState::RegisterOps(TaskState::EXITED,
        [this](CPUEUTask* task) { return this->onTaskDone(static_cast<SCPUEUTask*>(task)), true; });

#ifdef FFRT_WORKER_MONITOR
    static WorkerMonitor workerMonitor;
#endif
#ifdef FFRT_OH_TRACE_ENABLE
    _StartTrace(HITRACE_TAG_FFRT, "dm_init", -1); // init g_tagsProperty for ohos ffrt trace
    _FinishTrace(HITRACE_TAG_FFRT);
#endif
}

SDependenceManager::~SDependenceManager()
{
}

void SDependenceManager::onSubmit(bool has_handle, ffrt_task_handle_t &handle, ffrt_function_header_t *f,
    const ffrt_deps_t *ins, const ffrt_deps_t *outs, const task_attr_private *attr)
{
    FFRT_TRACE_SCOPE(1, onSubmit);
    // 1 Init eu and scheduler
    auto ctx = ExecuteCtx::Cur();
    auto en = Entity::Instance();

    // 2 Get current task's parent
    auto parent = (ctx->task && ctx->task->type == ffrt_normal_task) ? ctx->task : DependenceManager::Root();

    std::vector<const void*> insNoDup;
    std::vector<const void*> outsNoDup;
    std::vector<CPUEUTask*> in_handles;
    // signature去重：1）outs去重
    if (outs) {
        if (!outsDeDup(outsNoDup, outs)) {
            FFRT_LOGE("onSubmit outsDeDup error");
            return;
        }
    }

    // signature去重：2）ins去重（不影响功能，skip）；3）ins不和outs重复（当前不支持weak signature）
    if (ins) {
        insDeDup(in_handles, insNoDup, outsNoDup, ins);
    }

    // 2.1 Create task ctx
    SCPUEUTask* task = nullptr;
    {
        FFRT_TRACE_SCOPE(1, CreateTask);
        task = reinterpret_cast<SCPUEUTask*>(static_cast<uintptr_t>(
            static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) - OFFSETOF(SCPUEUTask, func_storage)));
        new (task)SCPUEUTask(attr, parent, ++parent->childNum, QoS());
    }
    FFRT_LOGD("submit task[%lu], name[%s]", task->gid, task->label.c_str());
#ifdef ASYNC_STACKTRACE
    {
        task->stackId = FFRTCollectAsyncStack();
    }
#endif

#ifdef FFRT_HITRACE_ENABLE
    if (HiTraceChain::GetId().IsValid() && task != nullptr) {
        task->traceId_ = std::make_unique<HiTraceId>(HiTraceChain::CreateSpan());
        HiTraceChain::Tracepoint(HITRACE_TP_CS, *(task->traceId_), "ffrt::SDependenceManager::onSubmit");
    }
#endif

#ifdef FFRT_BBOX_ENABLE
    TaskSubmitCounterInc();
#endif
#ifdef FFRT_OH_WATCHDOG_ENABLE
    if (attr != nullptr && IsValidTimeout(task->gid, attr->timeout_)) {
        task->isWatchdogEnable = true;
        AddTaskToWatchdog(task->gid);
        SendTimeoutWatchdog(task->gid, attr->timeout_, attr->delay_);
    }
#endif
    if (has_handle) {
        task->IncDeleteRef();
        handle = static_cast<ffrt_task_handle_t>(task);
        outsNoDup.push_back(handle); // handle作为任务的输出signature
    }
    QoS qos = (attr == nullptr ? QoS() : QoS(attr->qos_));
    task->SetQos(qos);
    /* The parent's number of subtasks to be completed increases by one,
        * and decreases by one after the subtask is completed
        */
    task->IncChildRef();

    std::vector<std::pair<VersionCtx*, NestType>> inDatas;
    std::vector<std::pair<VersionCtx*, NestType>> outDatas;

    if (!(insNoDup.empty() && outsNoDup.empty())) {
        // 3 Put the submitted task into Entity
        FFRT_TRACE_SCOPE(1, submitBeforeLock);
        std::lock_guard<decltype(criticalMutex_)> lg(criticalMutex_);
        FFRT_TRACE_SCOPE(1, submitAfterLock);

        MapSignature2Deps(task, insNoDup, outsNoDup, inDatas, outDatas);

        {
            FFRT_TRACE_SCOPE(1, dealInDepend);
            // 3.1 Process input dependencies
            for (auto& i : std::as_const(inDatas)) {
                i.first->AddConsumer(task, i.second);
            }
        }

        {
            FFRT_TRACE_SCOPE(1, dealOutDepend);
            // 3.2 Process output dependencies
            for (auto& o : std::as_const(outDatas)) {
                o.first->AddProducer(task);
            }
        }
        task->in_handles.swap(in_handles);
        if (task->depRefCnt != 0) {
            FFRT_BLOCK_TRACER(task->gid, dep);

#ifdef FFRT_HITRACE_ENABLE
            if (task != nullptr && task->traceId_ != nullptr) {
                HiTraceChain::Tracepoint(HITRACE_TP_CR, *(task->traceId_), "ffrt::SDependenceManager::onSubmit");
            }
#endif

            return;
        }
    }

#ifdef FFRT_HITRACE_ENABLE
    if (task != nullptr && task->traceId_ != nullptr) {
        HiTraceChain::Tracepoint(HITRACE_TP_CR, *(task->traceId_), "ffrt::SDependenceManager::onSubmit");
    }
#endif

    FFRT_LOGD("Submit completed, enter ready queue, task[%lu], name[%s]", task->gid, task->label.c_str());
    task->UpdateState(TaskState::READY);
#ifdef FFRT_BBOX_ENABLE
    TaskEnQueuCounterInc();
#endif
}

void SDependenceManager::onWait()
{
    auto ctx = ExecuteCtx::Cur();
    auto baseTask = ctx->task ? ctx->task : DependenceManager::Root();
    auto task = static_cast<SCPUEUTask*>(baseTask);
    bool legacyMode = LegacyMode(task);
    if (!USE_COROUTINE || task->parent == nullptr || legacyMode) {
        std::unique_lock<std::mutex> lck(task->lock);
        task->MultiDepenceAdd(Denpence::CALL_DEPENCE);
        FFRT_LOGD("onWait name:%s gid=%lu", task->label.c_str(), task->gid);
        if (legacyMode) {
            task->coRoutine->blockType = BlockType::BLOCK_THREAD;
        }
        task->childWaitCond_.wait(lck, [task] { return task->childWaitRefCnt == 0; });
        return;
    }

    auto childDepFun = [&](ffrt::CPUEUTask* task) -> bool {
        auto sTask = static_cast<SCPUEUTask*>(task);
        std::unique_lock<std::mutex> lck(sTask->lock);
        if (sTask->childWaitRefCnt == 0) {
            return false;
        }
        sTask->MultiDepenceAdd(Denpence::CALL_DEPENCE);
        sTask->UpdateState(ffrt::TaskState::BLOCKED);
        return true;
    };
    FFRT_BLOCK_TRACER(task->gid, chd);
    CoWait(childDepFun);
}

#ifdef QOS_DEPENDENCY
void SDependenceManager::onWait(const ffrt_deps_t* deps, int64_t deadline = -1)
#else
void SDependenceManager::onWait(const ffrt_deps_t* deps)
#endif
{
    auto ctx = ExecuteCtx::Cur();
    auto baseTask = (ctx->task && ctx->task->type == ffrt_normal_task) ? ctx->task : DependenceManager::Root();
    auto task = static_cast<SCPUEUTask*>(baseTask);

    auto dataDepFun = [&]() {
        std::vector<VersionCtx*> waitDatas;
        waitDatas.reserve(deps->len);
        std::lock_guard<decltype(criticalMutex_)> lg(criticalMutex_);

        for (uint32_t i = 0; i < deps->len; ++i) {
            auto d = deps->items[i].ptr;
            auto it = std::as_const(Entity::Instance()->vaMap).find(d);
            if (it != Entity::Instance()->vaMap.end()) {
                auto waitData = it->second;
                // Find the VersionCtx of the parent task level
                for (auto out : std::as_const(task->outs)) {
                    if (waitData->signature == out->signature) {
                        waitData = out;
                        break;
                    }
                }
                waitDatas.push_back(waitData);
            }
        }
#ifdef QOS_DEPENDENCY
        if (deadline != -1) {
            Scheduler::Instance()->onWait(waitDatas, deadline);
        }
#endif
        for (auto data : std::as_const(waitDatas)) {
            data->AddDataWaitTaskByThis(task);
        }
    };

    bool legacyMode = LegacyMode(task);
    if (!USE_COROUTINE || task->parent == nullptr || legacyMode) {
        dataDepFun();
        std::unique_lock<std::mutex> lck(task->lock);
        task->MultiDepenceAdd(Denpence::DATA_DEPENCE);
        FFRT_LOGD("onWait name:%s gid=%lu", task->label.c_str(), task->gid);
        if (legacyMode) {
            task->coRoutine->blockType = BlockType::BLOCK_THREAD;
        }
        task->dataWaitCond_.wait(lck, [task] { return task->dataWaitRefCnt == 0; });
        return;
    }

    auto pendDataDepFun = [&](ffrt::CPUEUTask* task) -> bool {
        auto sTask = static_cast<SCPUEUTask*>(task);
        dataDepFun();
        FFRT_LOGD("onWait name:%s gid=%lu", sTask->label.c_str(), sTask->gid);
        std::unique_lock<std::mutex> lck(sTask->lock);
        if (sTask->dataWaitRefCnt == 0) {
            return false;
        }
        sTask->MultiDepenceAdd(Denpence::DATA_DEPENCE);
        sTask->UpdateState(ffrt::TaskState::BLOCKED);
        return true;
    };
    FFRT_BLOCK_TRACER(task->gid, dat);
    CoWait(pendDataDepFun);
}

void SDependenceManager::onTaskDone(CPUEUTask* task)
{
    auto sTask = static_cast<SCPUEUTask*>(task);
    FFRT_LOGD("Task completed, task[%lu], name[%s]", sTask->gid, sTask->label.c_str());
#ifdef FFRT_BBOX_ENABLE
    TaskDoneCounterInc();
#endif
    FFRT_TRACE_SCOPE(1, ontaskDone);
    sTask->DecChildRef();
    if (!(sTask->ins.empty() && sTask->outs.empty())) {
        std::lock_guard<decltype(criticalMutex_)> lg(criticalMutex_);
        FFRT_TRACE_SCOPE(1, taskDoneAfterLock);

        // Production data
        for (auto out : std::as_const(sTask->outs)) {
            out->onProduced();
        }
        // Consumption data
        for (auto in : std::as_const(sTask->ins)) {
            in->onConsumed(sTask);
        }
        for (auto in : std::as_const(sTask->in_handles)) {
            in->DecDeleteRef();
        }
        // VersionCtx recycling
        Entity::Instance()->RecycleVersion();
    }
    if (task->isWatchdogEnable) {
        RemoveTaskFromWatchdog(task->gid);
    }
    sTask->RecycleTask();
}

void SDependenceManager::MapSignature2Deps(SCPUEUTask* task, const std::vector<const void*>& inDeps,
    const std::vector<const void*>& outDeps, std::vector<std::pair<VersionCtx*, NestType>>& inVersions,
    std::vector<std::pair<VersionCtx*, NestType>>& outVersions)
{
    auto en = Entity::Instance();
    // scene description：
    for (auto signature : inDeps) {
        VersionCtx* version = nullptr;
        NestType type = NestType::DEFAULT;
        // scene 1|2
        for (auto parentOut : std::as_const(static_cast<SCPUEUTask*>(task->parent)->outs)) {
            if (parentOut->signature == signature) {
                version = parentOut;
                type = NestType::PARENTOUT;
                goto add_inversion;
            }
        }
        // scene 3
        for (auto parentIn : std::as_const(static_cast<SCPUEUTask*>(task->parent)->ins)) {
            if (parentIn->signature == signature) {
                version = parentIn;
                type = NestType::PARENTIN;
                goto add_inversion;
            }
        }
        // scene 4
        version = en->VA2Ctx(signature, task);
    add_inversion:
        inVersions.push_back({version, type});
    }

    for (auto signature : outDeps) {
        VersionCtx* version = nullptr;
        NestType type = NestType::DEFAULT;
        // scene 5|6
        for (auto parentOut : std::as_const(static_cast<SCPUEUTask*>(task->parent)->outs)) {
            if (parentOut->signature == signature) {
                version = parentOut;
                type = NestType::PARENTOUT;
                goto add_outversion;
            }
        }
        // scene 7
#ifndef FFRT_RELEASE
        for (auto parentIn : std::as_const(static_cast<SCPUEUTask*>(task->parent)->ins)) {
            if (parentIn->signature == signature) {
                FFRT_LOGE("parent's indep only cannot be child's outdep");
            }
        }
#endif
        // scene 8
        version = en->VA2Ctx(signature, task);
    add_outversion:
        outVersions.push_back({version, type});
    }
}
} // namespace ffrt