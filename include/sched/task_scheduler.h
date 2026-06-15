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

#ifndef FFRT_TASK_SCHEDULER_HPP
#define FFRT_TASK_SCHEDULER_HPP

#include "sched/task_runqueue.h"
#include "tm/task_base.h"
#include "util/spmc_queue.h"
#include "tm/uv_task.h"

namespace ffrt {
class TaskScheduler {
public:
    TaskScheduler() = default;
    virtual ~TaskScheduler() {}

    virtual bool PushTask(TaskBase* task, bool rtb) = 0;

    virtual TaskBase* PopTask() = 0;

    virtual void SetQos(QoS &q) = 0;

    int qos {0};

    // global_queue.size + totalLocalTaskCnt, not include the PriorityTaskCnt
    virtual uint64_t GetTotalTaskCnt()
    {
        return GetGlobalTaskCnt();
    }
    // global_queue.size
    virtual uint64_t GetGlobalTaskCnt() = 0;

    // global_queue.size
    virtual uint64_t GetRTQTaskCnt() = 0;

    // global_queue is empty?
    virtual bool GlobalTaskEmpty() = 0;

    bool CancelUVWork(ffrt_executor_task_t* uvWork);
    bool PushUVTaskToWaitingQueue(UVTask* task);
    bool CheckUVTaskConcurrency(UVTask* task);
    UVTask* PickWaitingUVTask();

    std::mutex* GetMutex();

    inline bool IsStealerActive()
    {
        return stealingInProgress.load(std::memory_order_relaxed);
    }

protected:

    TaskBase* GetUVTask(TaskBase* task)
    {
        std::lock_guard<std::mutex> lg(uvMtx);
        UVTask* uvTask = static_cast<UVTask*>(task);
        auto it = cancelSet_.find(uvTask->uvWork);
        if (it != cancelSet_.end()) {
            uvTask->FreeMem();
            cancelSet_.erase(it);
            return nullptr;
        }

        uvTask->SetDequeued();
        return task;
    }

private:
    std::atomic<std::mutex*> mtx {nullptr};
    std::mutex uvMtx;
    std::set<ffrt_executor_task_t*> cancelSet_;
    int uvTaskConcurrency_ = 0;
    std::deque<UVTask*> uvTaskWaitingQueue_;
    std::atomic<bool> stealingInProgress { false }; /* indicates whether a stealer is in progress or not */
};

class SchedulerFactory {
public:
    using AllocCB = std::function<TaskScheduler*()>;
    using RecycleCB = std::function<void (TaskScheduler*)>;

    static SchedulerFactory& Instance();

    static TaskScheduler *Alloc()
    {
        return Instance().alloc_();
    }

    static void Recycle(TaskScheduler *schd)
    {
        Instance().recycle_(schd);
    }

    static void RegistCb(const AllocCB &alloc, const RecycleCB &recycle)
    {
        Instance().alloc_ = alloc;
        Instance().recycle_ = recycle;
    }

private:
    AllocCB alloc_;
    RecycleCB recycle_;
};
} // namespace ffrt

#endif
