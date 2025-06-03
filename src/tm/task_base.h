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

#ifndef _TASK_BASE_H_
#define _TASK_BASE_H_
#include <atomic>
#include <vector>
#include "eu/co_routine.h"
#include "internal_inc/types.h"
#include "qos.h"
#include "sched/execute_ctx.h"
#include "internal_inc/types.h"
#include "core/task_attr_private.h"
#include "internal_inc/non_copyable.h"
#include "util/time_format.h"

namespace ffrt {
static constexpr uint64_t cacheline_size = 64;

typedef struct HiTraceIdStruct {
#if __BYTE_ORDER == __BIG_ENDIAN
    uint64_t chainId : 60;
    uint64_t ver : 3;
    uint64_t valid : 1;

    uint64_t parentSpanId : 26;
    uint64_t spanId : 26;
    uint64_t flags : 12;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    uint64_t valid : 1;
    uint64_t ver : 3;
    uint64_t chainId : 60;

    uint64_t flags : 12;
    uint64_t spanId : 26;
    uint64_t parentSpanId : 26;
#else
#error "ERROR: No BIG_LITTLE_ENDIAN defines."
#endif
} HiTraceIdStruct;
constexpr uint64_t HITRACE_ID_VALID = 1;

class TaskBase : private NonCopyable {
public:
    TaskBase(ffrt_executor_task_type_t type, const task_attr_private *attr);
    virtual ~TaskBase() = default;

    // lifecycle actions
    virtual void Submit() = 0;
    virtual void Ready() = 0;
    virtual void Pop() = 0;
    virtual void Cancel() = 0;
    virtual void Finish() = 0;
    virtual void Execute() = 0;
    virtual void FreeMem() = 0;

    // getters and setters
    virtual std::string GetLabel() const = 0;
    virtual void SetQos(const QoS& newQos) = 0;

    inline int GetQos() const
    {
        return qos_();
    }

    // delete ref setter functions, for memory management
    inline uint32_t IncDeleteRef()
    {
        auto v = rc.fetch_add(1);
        return v;
    }

    inline uint32_t DecDeleteRef()
    {
        auto v = rc.fetch_sub(1);
        if (v == 1) {
            FreeMem();
        }
        return v;
    }

    // returns the current g_taskId value
    static uint32_t GetLastGid();

    // properties
    WaitEntry fq_we; // used on fifo fast que
    ffrt_executor_task_type_t type;
    const uint64_t gid; // global unique id in this process
    QoS qos_ = qos_inherit;
    std::atomic_uint32_t rc = 1; // reference count for delete
    TaskStatus status = TaskStatus::PENDING;

#ifdef FFRT_ASYNC_STACKTRACE
    uint64_t stackId = 0;
#endif

    struct HiTraceIdStruct traceId_ = {};

    uint64_t createTime {0};
    uint64_t executeTime {0};
    int32_t fromTid {0};
};

class CoTask : public TaskBase {
public:
    CoTask(ffrt_executor_task_type_t type, const task_attr_private *attr)
        : TaskBase(type, attr)
    {
        if (attr) {
            stack_size = std::max(attr->stackSize_, MIN_STACK_SIZE);
        }
    }
    ~CoTask() override = default;

    std::string label;
    CoWakeType coWakeType { CoWakeType::NO_TIMEOUT_WAKE };
    int cpuBoostCtxId = -1;
    WaitUntilEntry* wue = nullptr;
    // lifecycle connection between task and coroutine is shown as below:
    // |*task pending*|*task ready*|*task executing*|*task done*|*task release*|
    //                             |**********coroutine*********|
    CoRoutine* coRoutine = nullptr;
    uint64_t stack_size = STACK_SIZE;
    std::atomic<pthread_t> runningTid = 0;
    int legacyCountNum = 0; // dynamic switch controlled by set_legacy_mode api
    BlockType blockType { BlockType::BLOCK_COROUTINE }; // block type for lagacy mode changing
    std::mutex mutex_; // used in coroute
    std::condition_variable waitCond_; // cv for thread wait
    std::atomic<CoTaskStatus> curStatus = CoTaskStatus::PENDING;
    CoTaskStatus preStatus = CoTaskStatus::PENDING;
    uint64_t statusTime = TimeStampCntvct();

    bool pollerEnable = false; // set true if task call ffrt_epoll_ctl
    std::string GetLabel() const override
    {
        return label;
    }

    void SetStatus(CoTaskStatus statusIn)
    {
        statusTime = TimeStampCntvct();
        preStatus = curStatus;
        curStatus = statusIn;
    }
};

void ExecuteTask(TaskBase* task);

inline bool IsCoTask(TaskBase* task)
{
    return task && (task->type == ffrt_normal_task || task->type == ffrt_queue_task);
}

inline bool LegacyMode(TaskBase* task)
{
    return IsCoTask(task) && (static_cast<CoTask*>(task)->legacyCountNum > 0);
}

inline bool BlockThread(TaskBase* task)
{
    return IsCoTask(task) && static_cast<CoTask*>(task)->blockType == BlockType::BLOCK_THREAD;
}

inline bool ThreadNotifyMode(TaskBase* task)
{
    if constexpr(!USE_COROUTINE) {
        // static switch controlled by macro
        return true;
    }
    if (!IsCoTask(task) || BlockThread(task)) {
        // thread wait happended when task in legacy mode
        return true;
    }
    return false;
}
} // namespace ffrt
#endif
