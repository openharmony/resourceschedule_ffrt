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
#include <unistd.h>
#include "task_base.h"
#include "sched/task_state.h"
#include "eu/co_routine.h"
#include "core/task_attr_private.h"
#include "dfx/log/ffrt_log_api.h"
#include "eu/func_manager.h"
#ifdef FFRT_ASYNC_STACKTRACE
#include "dfx/async_stack/ffrt_async_stack.h"
#endif

namespace ffrt {
constexpr int CO_CREATE_RETRY_INTERVAL = 500 * 1000;
constexpr uint64_t MASK_FOR_HCS_TASK = 0xFF000000000000;
struct VersionCtx;
class SCPUEUTask;

class CPUEUTask : public CoTask {
public:
    CPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id);
    SkipStatus skipped = SkipStatus::SUBMITTED;

    uint8_t func_storage[ffrt_auto_managed_function_storage_size]; // 函数闭包、指针或函数对象
    CPUEUTask* parent = nullptr;
    const uint64_t rank = 0x0;
    std::mutex lock; // used in coroute
    std::vector<CPUEUTask*> in_handles;
    TaskState state; // not used

    /* The current number of child nodes does not represent the real number of child nodes,
     * because the dynamic graph child nodes will grow to assist in the generation of id
     */
    std::atomic<uint64_t> childNum {0};
    bool isWatchdogEnable = false;
    bool notifyWorker_ = true;

    void** threadTsd = nullptr;
    void** tsd = nullptr;
    bool taskLocal = false;

    uint64_t reserved[8];

    inline bool IsRoot()
    {
        return parent == nullptr;
    }

    void Submit() override;
    void Ready() override;

    void Pop() override
    {
        status = TaskStatus::POPED;
    }

    void Execute() override;

    void Cancel() override
    {
        status = TaskStatus::CANCELED;
    }

    void FreeMem() override;
    void SetQos(const QoS& newQos) override;
};

inline bool ExecutedOnWorker(TaskBase* task)
{
    return task && (task->type != ffrt_normal_task || !static_cast<CPUEUTask*>(task)->IsRoot());
}

inline bool ThreadWaitMode(TaskBase* task)
{
    if constexpr(!USE_COROUTINE) {
        // static switch controlled by macro
        return true;
    }
    if (!IsCoTask(task) || !ExecutedOnWorker(task)) {
        // task is executed on user thread
        return true;
    }
    if (LegacyMode(task)) {
        // set_legacy_mode controlled by user
        return true;
    }
    return false;
}

inline bool NeedNotifyWorker(TaskBase* task)
{
    if (task == nullptr) {
        return false;
    }
    bool needNotify = true;
    if (task->type == ffrt_normal_task) {
        CPUEUTask* cpuTask = static_cast<CPUEUTask*>(task);
        needNotify = cpuTask->notifyWorker_;
        cpuTask->notifyWorker_ = true;
    }
    return needNotify;
}
} /* namespace ffrt */
#endif
