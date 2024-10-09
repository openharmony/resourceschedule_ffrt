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
#include "dfx/trace_record/ffrt_trace_record.h"
#include "util/worker_monitor.h"
#include "util/ffrt_facade.h"
#include "util/slab.h"
#include "tm/queue_task.h"

#ifdef FFRT_ASYNC_STACKTRACE
#include "dfx/async_stack/ffrt_async_stack.h"
#endif

namespace ffrt {

SDependenceManager::SDependenceManager() : criticalMutex_(Entity::Instance()->criticalMutex_)
{
#ifdef FFRT_OH_TRACE_ENABLE
    TraceAdapter::Instance();
#endif
    // control construct sequences of singletons
    SimpleAllocator<CPUEUTask>::Instance();
    SimpleAllocator<QueueTask>::Instance();
    SimpleAllocator<VersionCtx>::Instance();
    SimpleAllocator<WaitUntilEntry>::Instance();
    PollerProxy::Instance();
    FFRTScheduler::Instance();
    ExecuteUnit::Instance();
    TaskState::RegisterOps(TaskState::EXITED,
        [this](CPUEUTask* task) { return this->onTaskDone(static_cast<SCPUEUTask*>(task)), true; });

#ifdef FFRT_WORKER_MONITOR
    WorkerMonitor::GetInstance();
#endif
#ifdef FFRT_OH_TRACE_ENABLE
    _StartTrace(HITRACE_TAG_FFRT, "dm_init", -1); // init g_tagsProperty for ohos ffrt trace
    _FinishTrace(HITRACE_TAG_FFRT);
#endif
    DelayedWorker::GetInstance();
}

SDependenceManager::~SDependenceManager()
{
}

void SDependenceManager::RemoveRepeatedDeps(std::vector<CPUEUTask*>& in_handles, const ffrt_deps_t* ins, const ffrt_deps_t* outs,
    std::vector<const void *>& insNoDup, std::vector<const void *>& outsNoDup)
{
    // signature去重：1）outs去重
    if (outs) {
        OutsDedup(outsNoDup, outs);
    }

    // signature去重：2）ins去重（不影响功能，skip）；3）ins不和outs重复（当前不支持weak signature）
    if (ins) {
        InsDedup(in_handles, insNoDup, outsNoDup, ins);
    }
}

void SDependenceManager::onSubmit(bool has_handle, ffrt_task_handle_t &handle, ffrt_function_header_t *f,
    const ffrt_deps_t *ins, const ffrt_deps_t *outs, const task_attr_private *attr)
{
    // 0 check outs handle
    if (!CheckOutsHandle(outs)) {
        FFRT_LOGE("outs contain handles error");
        return;
    }

    // 1 Init eu and scheduler
    auto ctx = ExecuteCtx::Cur();

    // 2 Get current task's parent
    auto parent = (ctx->task && ctx->task->type == ffrt_normal_task) ? ctx->task : DependenceManager::Root();

    // 2.1 Create task ctx
    SCPUEUTask* task = nullptr;
    {
        task = reinterpret_cast<SCPUEUTask*>(static_cast<uintptr_t>(
            static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) - OFFSETOF(SCPUEUTask, func_storage)));
        new (task)SCPUEUTask(attr, parent, ++parent->childNum, QoS());
    }
    FFRT_TRACE_BEGIN(("submit|" + std::to_string(task->gid)).c_str());
    FFRT_LOGD("submit task[%lu], name[%s]", task->gid, task->label.c_str());
#ifdef FFRT_ASYNC_STACKTRACE
    {
        task->stackId = FFRTCollectAsyncStack();
    }
#endif

    QoS qos = (attr == nullptr ? QoS() : QoS(attr->qos_));
    FFRTTraceRecord::TaskSubmit<ffrt_normal_task>(qos, &(task->createTime), &(task->fromTid));

    std::vector<const void*> insNoDup;
    std::vector<const void*> outsNoDup;
    RemoveRepeatedDeps(task->in_handles, ins, outs, insNoDup, outsNoDup);

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
    task->SetQos(qos);
    /* The parent's number of subtasks to be completed increases by one,
        * and decreases by one after the subtask is completed
        */
    task->IncChildRef();

    std::vector<std::pair<VersionCtx*, NestType>> inDatas;
    std::vector<std::pair<VersionCtx*, NestType>> outDatas;

    if (!(insNoDup.empty() && outsNoDup.empty())) {
        // 3 Put the submitted task into Entity
        std::lock_guard<decltype(criticalMutex_)> lg(criticalMutex_);

        MapSignature2Deps(task, insNoDup, outsNoDup, inDatas, outDatas);

        {
            // 3.1 Process input dependencies
            for (auto& i : std::as_const(inDatas)) {
                i.first->AddConsumer(task, i.second);
            }
        }

        {
            // 3.2 Process output dependencies
            for (auto& o : std::as_const(outDatas)) {
                o.first->AddProducer(task);
            }
        }
        if (task->dataRefCnt.submitDep != 0) {
            FFRT_BLOCK_TRACER(task->gid, dep);
            FFRT_TRACE_END();
            return;
        }
    }
    
    if (attr != nullptr) {
        task->notifyWorker_ = attr->notifyWorker_;
    }

    FFRT_LOGD("Submit completed, enter ready queue, task[%lu], name[%s]", task->gid, task->label.c_str());
    task->UpdateState(TaskState::READY);
    FFRTTraceRecord::TaskEnqueue<ffrt_normal_task>(qos);
    FFRT_TRACE_END();
}

void SDependenceManager::onWait()
{
    auto ctx = ExecuteCtx::Cur();
    auto baseTask = (ctx->task && ctx->task->type == ffrt_normal_task) ? ctx->task : DependenceManager::Root();
    auto task = static_cast<SCPUEUTask*>(baseTask);

    if (ThreadWaitMode(task)) {
        std::unique_lock<std::mutex> lck(task->mutex_);
        task->MultiDepenceAdd(Denpence::CALL_DEPENCE);
        FFRT_LOGD("onWait name:%s gid=%lu", task->label.c_str(), task->gid);
        if (FFRT_UNLIKELY(LegacyMode(task))) {
            task->blockType = BlockType::BLOCK_THREAD;
        }
        task->waitCond_.wait(lck, [task] { return task->childRefCnt == 0; });
        return;
    }

    auto childDepFun = [&](ffrt::CPUEUTask* task) -> bool {
        auto sTask = static_cast<SCPUEUTask*>(task);
        std::unique_lock<std::mutex> lck(sTask->mutex_);
        if (sTask->childRefCnt == 0) {
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
    task->dataRefCnt.waitDep = 0;

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

    if (ThreadWaitMode(task)) {
        dataDepFun();
        std::unique_lock<std::mutex> lck(task->mutex_);
        task->MultiDepenceAdd(Denpence::DATA_DEPENCE);
        FFRT_LOGD("onWait name:%s gid=%lu", task->label.c_str(), task->gid);
        if (FFRT_UNLIKELY(LegacyMode(task))) {
            task->blockType = BlockType::BLOCK_THREAD;
        }
        task->waitCond_.wait(lck, [task] { return task->dataRefCnt.waitDep == 0; });
        return;
    }

    auto pendDataDepFun = [&](ffrt::CPUEUTask* task) -> bool {
        auto sTask = static_cast<SCPUEUTask*>(task);
        dataDepFun();
        FFRT_LOGD("onWait name:%s gid=%lu", sTask->label.c_str(), sTask->gid);
        std::unique_lock<std::mutex> lck(sTask->mutex_);
        if (sTask->dataRefCnt.waitDep == 0) {
            return false;
        }
        sTask->MultiDepenceAdd(Denpence::DATA_DEPENCE);
        sTask->UpdateState(ffrt::TaskState::BLOCKED);
        return true;
    };
    FFRT_BLOCK_TRACER(task->gid, dat);
    CoWait(pendDataDepFun);
}

int SDependenceManager::onExecResults(ffrt_task_handle_t handle)
{
    return 0;
}

void SDependenceManager::onTaskDone(CPUEUTask* task)
{
    auto sTask = static_cast<SCPUEUTask*>(task);
    FFRTTraceRecord::TaskDone<ffrt_normal_task>(task->GetQos());
    FFRTTraceRecord::TaskDone<ffrt_normal_task>(task->GetQos(),  task);
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

int SDependenceManager::onSkip(ffrt_task_handle_t handle)
{
    FFRT_COND_DO_ERR((handle == nullptr), return ffrt_error_inval, "input ffrt task handle is invalid.");

    ffrt::CPUEUTask *task = static_cast<ffrt::CPUEUTask*>(handle);
    auto exp = ffrt::SkipStatus::SUBMITTED;
    if (__atomic_compare_exchange_n(&task->skipped, &exp, ffrt::SkipStatus::SKIPPED, 0, __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED)) {
        return ffrt_success;
    }

    FFRT_LOGE("skip task [%lu] failed", task->gid);    
    return ffrt_error;
}
} // namespace ffrt