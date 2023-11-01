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

struct TaskCtx : public TaskDeleter {
    TaskCtx(const task_attr_private* attr,
        TaskCtx* parent, const uint64_t& id, const char *identity = nullptr, const QoS& qos = QoS());
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
    std::mutex lock; // used in coroute
    uint64_t myRefCnt = 0;

    TaskState state;
    std::mutex denpenceStatusLock;
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

    QoS qos;
    void SetQos(QoS& newQos);

    void freeMem() override
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

    void SetTraceTag(const char* name)
    {
        traceTag.emplace_back(name);
    }

    void ClearTraceTag()
    {
        if (!traceTag.empty()) {
            traceTag.pop_back();
        }
    }

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    static void DumpTask(TaskCtx* task, std::string& stackInfo, uint8_t flag = 0); /* 0:hilog others:hiview */
#endif
};

// APP线程提交任务没有执行完，但线程已退出，该任务依赖的父任务为根任务已经释放
class RootTaskCtx : public TaskCtx {
public:
    RootTaskCtx(const task_attr_private* attr, TaskCtx* parent, const uint64_t& id,
        const char *identity = nullptr, const QoS& qos = QoS()) : TaskCtx(attr, parent, id, identity, qos)
    {
    }
    // static inline void OnChildRefCntZero(RootTaskCtx root) {    }
public:
    bool thread_exit = false;
};

class RootTaskCtxWrapper {
public:
    RootTaskCtxWrapper()
    {
        // Within an ffrt process, different threads may have different QoS interval
        task_attr_private task_attr;
        root = new RootTaskCtx {&task_attr, nullptr, 0, nullptr};
    }
    ~RootTaskCtxWrapper()
    {
        // if the son of root task is empty,
        // then, only this thread holds the root task, and it is time to free it when this thread exits
        // else, set the hold thread exit flag, and the task scheduler should free the root task when is son is zero

        // because the root task was accessed in works and this thread, so task mutex should be used.
        std::unique_lock<decltype(root->lock)> lck(root->lock);
        if (root->childWaitRefCnt == 0) {
            // unlock()内部lck记录锁的状态为非持有状态，析构时访问状态变量为非持有状态，则不访问实际持有的mutex
            // return之前的lck析构不产生UAF问题，因为return之前随着root析构，锁的内存被释放
            lck.unlock();
            delete root;
        } else {
            root->thread_exit = true;
        }
    }
    TaskCtx* Root()
    {
        return root;
    }
private:
    RootTaskCtx *root = nullptr;
};
} /* namespace ffrt */
#endif
