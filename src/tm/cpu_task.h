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
#include "eu/co_routine.h"
#include "core/task_attr_private.h"
#include "core/task_io.h"
#include "util/task_deleter.h"

#ifdef FFRT_HITRACE_ENABLE
#include "hitrace/trace.h"
#endif

namespace ffrt {
struct VersionCtx;
class SCPUEUTask;
#ifdef FFRT_IO_TASK_SCHEDULER
class UserDefinedTask : public TaskBase {
    ffrt_io_callable_t work;
    ExecTaskStatus status;
};
#endif

#define TSD_SIZE 128

class CPUEUTask : public CoTask {
public:
    CPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id, const QoS &qos);
    SkipStatus skipped = SkipStatus::SUBMITTED;
    TaskStatus status = TaskStatus::PENDING;

    uint8_t func_storage[ffrt_auto_managed_function_storage_size]; // 函数闭包、指针或函数对象
    CPUEUTask* parent = nullptr;
    const uint64_t rank = 0x0;
    std::mutex lock; // used in coroute

    TaskState state;

#ifdef FFRT_HITRACE_ENABLE
    std::unique_ptr<OHOS::HiviewDFX::HiTraceId> traceId_{nullptr};
#endif

    /* The current number of child nodes does not represent the real number of child nodes,
     * because the dynamic graph child nodes will grow to assist in the generation of id
     */
    std::atomic<uint64_t> childNum {0};
    bool isWatchdogEnable = false;

    void** threadTsd = nullptr;
    void** tsd = nullptr;
    bool taskLocal = false;

    QoS qos;
    void SetQos(QoS& newQos);
    uint64_t reserved[8];

    void FreeMem() override;
    void Execute() override;

    virtual void RecycleTask() = 0;
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
    static void DumpTask(CPUEUTask* task, std::string& stackInfo, uint8_t flag = 0); /* 0:hilog others:hiview */
#endif
};

inline bool LegacyMode(CPUEUTask* task)
{
    bool legacyMode = (task && task->coRoutine) ? task->coRoutine->legacyMode : false;
    return legacyMode;
}

inline bool BlockThread(CPUEUTask* task)
{
    bool blockThread = (task && task->coRoutine) ? task->coRoutine->blockType == BlockType::BLOCK_THREAD : false;
    return blockThread;
}
} /* namespace ffrt */
#endif
