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

#ifndef FFRT_TASK_CTX_H
#define FFRT_TASK_CTX_H

#include <string>
#include <functional>
#include <unordered_set>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <set>
#include <list>
#include <memory>
#include "internal_inc/types.h"
#include "sched/task_state.h"
#include "sched/interval.h"
#include "eu/co_routine.h"
#include "task_attr_private.h"
#include "util/slab.h"
#include "util/task_deleter.h"
#include "dfx/bbox/bbox.h"

namespace ffrt {
struct TaskCtx;
struct VersionCtx;

typedef struct{
    ffrt_function_ptr_t exec;
    ffrt_function_ptr_t destroy;
    void* callable;
}ffrt_callable_t;

struct TaskCtx : public TaskDeleter {
    TaskCtx(const task_attr_private* attr,
        TaskCtx* parent, const uint64_t& id, const char *identity = nullptr, const QoS& qos = QoS());
    WaitEntry fq_we; // used on fifo fast que
    WaitUntilEntry* wue;
    bool wakeupTimeOut = false;
    bool is_native_func = false;
    SkipStatus skipped = SkipStatus::SUBMITTED;

    uint8_t func_storage[ffrt_auto_managed_function_storage_size]; // 函数闭包、指针或函数对象
    TaskCtx* parent = nullptr;
    const uint64_t rank = 0x0;
    std::unordered_set<VersionCtx*> ins;
    std::unordered_set<VersionCtx*> outs;
    std::vector<TaskCtx*> in_handles;
    CoRoutine* coRoutine = nullptr;
    std::vector<std::string> traceTag;
    bool wakeFlag=true;
    ffrt_callable_t wake_callable_on_finish{nullptr,nullptr,nullptr};

#ifdef MUTEX_PERF // Mutex Lock&Unlock Cycles Statistic
    xx::mutex lock {"TaskCtx::lock"};
#else
    std::mutex lock; // used in coroute
#endif
    uint64_t myRefCnt = 0;

    TaskState state;
#ifdef MUTEX_PERF // Mutex Lock&Unlock Cycles Statistic
    xx::mutex denpenceStatusLock {"TaskCtx::denpenceStatusLock"};
#else
    std::mutex denpenceStatusLock;
#endif
    uint64_t pmuCntBegin = 0;
    uint64_t pmuCnt = 0;
    Denpence denpenceStatus {Denpence::DEPENCE_INIT};
    /* The current number of child nodes does not represent the real number of child nodes,
     * because the dynamic graph child nodes will grow to assist in the generation of id
     */
    std::atomic<uint64_t> childNum {0};
    std::atomic_uint64_t depRefCnt {0};

    std::atomic_uint64_t childWaitRefCnt {0};
    std::condition_variable childWaitCond_;

    uint64_t dataWaitRefCnt {0}; // waited data count called by ffrt_wait()
    std::condition_variable dataWaitCond_; // wait data cond
    TaskStatus status = TaskStatus::PENDING;

    void InitRelatedIntervals(TaskCtx* curr);
    std::unordered_set<Interval*> relatedIntervals;

    const char* identity;
    int64_t ddlSlack = INT64_MAX;
    uint64_t load = 0;
    int64_t ddl = INT64_MAX;

    const uint64_t gid; // global unique id in this process

    QoS qos;
    void ChargeQoSSubmit(const QoS& qos);

    ffrt_coroutine_t coroutine_type=ffrt_coroutine_stackfull;
    uint32_t stackless_coroutine_wake_count=1;

    inline void freeMem() override
    {
        BboxCheckAndFreeze();
        SimpleAllocator<TaskCtx>::freeMem(this);
    }

    inline void IncDepRef()
    {
        ++depRefCnt;
    }
    void DecDepRef();

    inline void IncChildRef()
    {
        ++(parent->childWaitRefCnt);
    }
    void DecChildRef();

    inline void IncWaitDataRef()
    {
        ++dataWaitRefCnt;
    }
    void DecWaitDataRef();

    void DealChildWait();
    void DealDataWait();
    void RecycleTask();
    bool IsPrevTask(const TaskCtx* task) const;
    void MultiDepenceAdd(Denpence depType);

    std::string label; // used for debug

    inline bool IsRoot()
    {
        if (parent == nullptr) {
            return true;
        }
        return false;
    }

    int UpdateState(TaskState::State taskState)
    {
        return TaskState::OnTransition(taskState, this);
    }

    int UpdateState(TaskState::State taskState, TaskState::Op&& op)
    {
        return TaskState::OnTransition(taskState, this, std::move(op));
    }

    void SetTraceTag(const std::string& name)
    {
        traceTag.push_back(name);
    }

    void ClearTraceTag()
    {
        if (!traceTag.empty()) {
            traceTag.pop_back();
        }
    }

    void SetWakeFlag(bool wakeFlagIn)
    {
        wakeFlag=wakeFlagIn;
    }
    
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    static void DumpTask(TaskCtx* task);
#endif
};
} /* namespace ffrt */
#endif
