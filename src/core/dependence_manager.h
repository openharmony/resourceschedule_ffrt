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

#ifndef FFRT_DYNAMIC_GRAPH_H
#define FFRT_DYNAMIC_GRAPH_H
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>
#include "internal_inc/types.h"
#include "internal_inc/osal.h"
#include "core/task_ctx.h"
#include "core/version_ctx.h"
#include "sched/execute_ctx.h"
#include "sched/qos.h"
#include "dfx/trace/ffrt_trace.h"
#include "util/slab.h"
#include "sched/task_state.h"
#include "sched/scheduler.h"
#include "eu/execute_unit.h"
#include "entity.h"
#include "dfx/bbox/bbox.h"

namespace ffrt {
#define OFFSETOF(TYPE, MEMBER) (reinterpret_cast<size_t>(&((reinterpret_cast<TYPE *>(0))->MEMBER)))

using TaskCtxAllocator = SimpleAllocator<TaskCtx>;

inline bool outsDeDup(std::vector<const void *> &outsNoDup, const ffrt_deps_t *outs)
{
    for (uint32_t i = 0; i < outs->len; i++) {
        if (std::find(outsNoDup.begin(), outsNoDup.end(), outs->items[i].ptr) == outsNoDup.end()) {
            if ((outs->items[i].type) == ffrt_dependence_task) {
                FFRT_LOGE("handle can't be used as out dependence");
                return false;
            }
            outsNoDup.push_back(outs->items[i].ptr);
        }
    }
    return true;
}

inline void insDeDup(std::vector<TaskCtx*> &in_handles, std::vector<const void *> &insNoDup,
    std::vector<const void *> &outsNoDup, const ffrt_deps_t *ins)
{
    for (uint32_t i = 0; i < ins->len; i++) {
        if (std::find(outsNoDup.begin(), outsNoDup.end(), ins->items[i].ptr) == outsNoDup.end()) {
            if ((ins->items[i].type) == ffrt_dependence_task) {
                ((ffrt::TaskCtx*)(ins->items[i].ptr))->IncDeleteRef();
                in_handles.emplace_back((ffrt::TaskCtx*)(ins->items[i].ptr));
            }
            insNoDup.push_back(ins->items[i].ptr);
        }
    }
}

class DependenceManager {
public:
    DependenceManager() : criticalMutex_(Entity::Instance()->criticalMutex_)
    {
        // control construct sequences of singletons
        SimpleAllocator<TaskCtx>::instance();
        SimpleAllocator<VersionCtx>::instance();
        FFRTScheduler::Instance();
        ExecuteUnit::Instance();

        TaskState::RegisterOps(TaskState::EXITED, [this](TaskCtx* task) { return this->onTaskDone(task), true; });
    }
    ~DependenceManager()
    {
    }
    static inline DependenceManager* Instance()
    {
        static DependenceManager graph;
        return &graph;
    }

    static inline TaskCtx* Root()
    {
        // Within an ffrt process, different threads may have different QoS interval
        task_attr_private task_attr;
        thread_local static TaskCtx root {&task_attr, nullptr, 0, nullptr};
        return &root;
    }

    template <int WITH_HANDLE>
    void onSubmit(ffrt_task_handle_t &handle, ffrt_function_header_t *f, const ffrt_deps_t *ins,
        const ffrt_deps_t *outs, const task_attr_private *attr)
    {
        FFRT_TRACE_SCOPE(1, onSubmit);
        // 1 Init eu and scheduler
        auto ctx = ExecuteCtx::Cur();
        auto en = Entity::Instance();

        // 2 Get current task's parent
        auto parent = ctx->task ? ctx->task : DependenceManager::Root();

        std::vector<const void*> insNoDup;
        std::vector<const void*> outsNoDup;
        std::vector<TaskCtx*> in_handles;
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
        TaskCtx* task = nullptr;
        {
            FFRT_TRACE_SCOPE(1, CreateTask);
            task = reinterpret_cast<TaskCtx*>(static_cast<uintptr_t>(
                static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) - OFFSETOF(TaskCtx, func_storage)));
            new (task)TaskCtx(attr, parent, ++parent->childNum, nullptr);
        }
        FFRT_LOGD("submit task[%lu], name[%s]", task->gid, task->label.c_str());
#ifdef FFRT_BBOX_ENABLE
        TaskSubmitCounterInc();
#endif
        if (WITH_HANDLE != 0) {
            task->IncDeleteRef();
            handle = static_cast<ffrt_task_handle_t>(task);
            outsNoDup.push_back(handle); // handle作为任务的输出signature
        }
        QoS qos = (attr == nullptr ? QoS() : QoS(attr->qos_));
        task->ChargeQoSSubmit(qos);
        task->InitRelatedIntervals(parent);
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
                return;
            }
        }

        FFRT_LOGD("Submit completed, enter ready queue, task[%lu], name[%s]", task->gid, task->label.c_str());
        task->UpdateState(TaskState::READY);
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
    }

    void onSubmitUV(ffrt_executor_task_t* task, const task_attr_private* attr)
    {
        FFRT_TRACE_SCOPE(1, onSubmitUV);
#ifdef FFRT_BBOX_ENABLE
        TaskSubmitCounterInc();
#endif
        QoS qos = (attr == nullptr ? QoS() : QoS(attr->qos_));

        LinkedList* node = (LinkedList *)(&task->wq);
        FFRTScheduler* sch = FFRTScheduler::Instance();
        if (!sch->InsertNode(node, qos)) {
            FFRT_LOGE("Submit UV task failed!");
            return;
        }
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
    }

    void onWait()
    {
        auto ctx = ExecuteCtx::Cur();
        auto task = ctx->task ? ctx->task : DependenceManager::Root();
#ifdef EU_COROUTINE
        if (task->parent == nullptr)
#endif
        {
            std::unique_lock<std::mutex> lck(task->lock);
            task->MultiDepenceAdd(Denpence::CALL_DEPENCE);
            FFRT_LOGD("onWait name:%s gid=%lu", task->label.c_str(), task->gid);
            task->childWaitCond_.wait(lck, [task] { return task->childWaitRefCnt == 0; });
            return;
        }
#ifdef EU_COROUTINE
        auto childDepFun = [&](ffrt::TaskCtx* inTask) -> bool {
            std::unique_lock<std::mutex> lck(inTask->lock);
            if (inTask->childWaitRefCnt == 0) {
                return false;
            }
            inTask->MultiDepenceAdd(Denpence::CALL_DEPENCE);
            inTask->UpdateState(ffrt::TaskState::BLOCKED);
            return true;
        };
        FFRT_BLOCK_TRACER(task->gid, chd);
        CoWait(childDepFun);
#endif
    }

#ifdef QOS_DEPENDENCY
    void onWait(const ffrt_deps_t* deps, int64_t deadline = -1)
#else
    void onWait(const ffrt_deps_t* deps)
#endif
    {
        auto ctx = ExecuteCtx::Cur();
        auto task = ctx->task ? ctx->task : DependenceManager::Root();

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
#ifdef EU_COROUTINE
        if (task->parent == nullptr)
#endif
        {
            dataDepFun();
            std::unique_lock<std::mutex> lck(task->lock);
            task->MultiDepenceAdd(Denpence::DATA_DEPENCE);
            FFRT_LOGD("onWait name:%s gid=%lu", task->label.c_str(), task->gid);
            task->dataWaitCond_.wait(lck, [task] { return task->dataWaitRefCnt == 0; });
            return;
        }
#ifdef EU_COROUTINE
        auto pendDataDepFun = [&](ffrt::TaskCtx* inTask) -> bool {
            dataDepFun();
            FFRT_LOGD("onWait name:%s gid=%lu", inTask->label.c_str(), inTask->gid);
            std::unique_lock<std::mutex> lck(inTask->lock);
            if (inTask->dataWaitRefCnt == 0) {
                return false;
            }
            inTask->MultiDepenceAdd(Denpence::DATA_DEPENCE);
            inTask->UpdateState(ffrt::TaskState::BLOCKED);
            return true;
        };
        FFRT_BLOCK_TRACER(task->gid, dat);
        CoWait(pendDataDepFun);
#endif
    }

    void onTaskDone(TaskCtx* task)
    {
        FFRT_LOGD("Task completed, task[%lu], name[%s]", task->gid, task->label.c_str());
#ifdef FFRT_BBOX_ENABLE
        TaskDoneCounterInc();
#endif
        FFRT_TRACE_SCOPE(1, ontaskDone);
        task->DecChildRef();
        if (!(task->ins.empty() && task->outs.empty())) {
            std::lock_guard<decltype(criticalMutex_)> lg(criticalMutex_);
            FFRT_TRACE_SCOPE(1, taskDoneAfterLock);

            // Production data
            for (auto out : std::as_const(task->outs)) {
                out->onProduced();
            }
            // Consumption data
            for (auto in : std::as_const(task->ins)) {
                in->onConsumed(task);
            }
            for (auto in : std::as_const(task->in_handles)) {
                in->DecDeleteRef();
            }

            // VersionCtx recycling
            Entity::Instance()->RecycleVersion();
        }

        task->RecycleTask();
    }

    void MapSignature2Deps(TaskCtx* task, const std::vector<const void*>& inDeps,
        const std::vector<const void*>& outDeps, std::vector<std::pair<VersionCtx*, NestType>>& inVersions,
        std::vector<std::pair<VersionCtx*, NestType>>& outVersions)
    {
        auto en = Entity::Instance();
        // scene description：
        for (auto signature : inDeps) {
            VersionCtx* version = nullptr;
            NestType type = NestType::DEFAULT;
            // scene 1|2
            for (auto parentOut : std::as_const(task->parent->outs)) {
                if (parentOut->signature == signature) {
                    version = parentOut;
                    type = NestType::PARENTOUT;
                    goto add_inversion;
                }
            }
            // scene 3
            for (auto parentIn : std::as_const(task->parent->ins)) {
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
            for (auto parentOut : std::as_const(task->parent->outs)) {
                if (parentOut->signature == signature) {
                    version = parentOut;
                    type = NestType::PARENTOUT;
                    goto add_outversion;
                }
            }
            // scene 7
#ifndef FFRT_RELEASE
            for (auto parentIn : std::as_const(task->parent->ins)) {
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

#ifdef MUTEX_PERF // Mutex Lock&Unlock Cycles Statistic
    xx::mutex& criticalMutex_;
#else
    fast_mutex& criticalMutex_;
#endif
};
} // namespace ffrt
#endif
