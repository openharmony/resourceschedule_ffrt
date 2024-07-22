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
#ifndef _CPU_TASK_H_
#define _CPU_TASK_H_

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
#include "task_base.h"
#include "sched/interval.h"
#include "sched/task_state.h"
#include "eu/co_routine.h"
#include "core/task_attr_private.h"
#include "core/task_io.h"
#include "util/task_deleter.h"

namespace ffrt {
struct VersionCtx;
class SCPUEUTask;
class UserDefinedTask : public TaskBase {
    ffrt_io_callable_t work;
    ExecTaskStatus status;
};

class CPUEUTask : public CoTask {
public:
    CPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id, const QoS &qos);
    SkipStatus skipped = SkipStatus::SUBMITTED;
    TaskStatus status = TaskStatus::PENDING;

    uint8_t func_storage[ffrt_auto_managed_function_storage_size]; // 函数闭包、指针或函数对象
    CPUEUTask* parent = nullptr;
    const uint64_t rank = 0x0;
    std::mutex lock; // used in coroute
    std::vector<CPUEUTask*> in_handles;
    TaskState state;

    /* The current number of child nodes does not represent the real number of child nodes,
     * because the dynamic graph child nodes will grow to assist in the generation of id
     */
    std::atomic<uint64_t> childNum {0};
    bool isWatchdogEnable = false;
    bool notifyWorker_ = true;

    void** threadTsd = nullptr;
    void** tsd = nullptr;
    bool taskLocal = false;

    bool legacyMode { false }; // dynamic switch controlled by set_legacy_mode api
    BlockType blockType { BlockType::BLOCK_COROUTINE }; // block type for lagacy mode changing

    QoS qos;
    void SetQos(QoS& newQos);
    uint64_t reserved[8];

    void FreeMem() override;
    void Execute() override;

    virtual void RecycleTask() = 0;

    inline bool IsRoot()
    {
        return parent == nullptr;
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
};

inline bool ExecutedOnWorker(CPUEUTask* task)
{
    return task && !task->IsRoot();
}

inline bool LegacyMode(CPUEUTask* task)
{
    return task && task->legacyMode;
}

inline bool BlockThread(CPUEUTask* task)
{
    return task && task->blockType == BlockType::BLOCK_THREAD;
}

inline bool ThreadWaitMode(CPUEUTask* task)
{
    if constexpr(!USE_COROUTINE) {
        // static switch controlled by macro
        return true;
    }
    if (!ExecutedOnWorker(task)) {
        // task is executed on user thread
        return true;
    }
    if (LegacyMode(task)) {
        // set_legacy_mode controlled by user
        return true;
    }
    return false;
}

inline bool ThreadNotifyMode(CPUEUTask* task)
{
    if constexpr(!USE_COROUTINE) {
        // static switch controlled by macro
        return true;
    }
    if (BlockThread(task)) {
        // thread wait happended when task in legacy mode
        return true;
    }
    return false;
}
} /* namespace ffrt */
#endif
