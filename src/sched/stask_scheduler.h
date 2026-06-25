/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifndef FFRT_STASK_SCHEDULER_HPP
#define FFRT_STASK_SCHEDULER_HPP
#include "sched/task_scheduler.h"
#include "dfx/trace/ffrt_trace.h"
#include "tm/uv_task.h"
#include "util/ffrt_facade.h"

namespace ffrt {
class STaskScheduler : public TaskScheduler {
public:
    STaskScheduler()
    {
        que = std::make_unique<FIFOQueue>();
    }

    void SetQos(QoS &q) override
    {
        qos = q;
        mtx = &g_schedMtx[qos];
    }

    uint64_t GetGlobalTaskCnt() override
    {
        return que->Size();
    }

    virtual uint64_t GetRTQTaskCnt() override
    {
        return que->Size();
    }

    virtual bool GlobalTaskEmpty() override
    {
        return que->Empty();
    }

    bool PushTask(TaskBase* task, bool rtb) override
    {
        constexpr int TASK_OVERRUN_THRESHOLD = 1000;
        constexpr int TASK_OVERRUN_ALARM_FREQ = 500;
        FFRT_PERF_TRACE_SCOPED_BY_GROUP(SCHED, STaskScheduler_PushTaskGlobal, DEFAULT_CONFIG);
        (void)rtb; // rtb is deprecated here
        FFRT_COND_DO_ERR((task == nullptr), return false, "task is nullptr");

        int taskCount = 0;
        int level = task->GetQos();
        uint64_t gid = task->gid;
        std::string label = task->GetLabel();

        FFRT_READY_MARKER(gid); // ffrt normal task ready to enqueue
        {
            std::lock_guard lg(*mtx);
            // enqueue task and read size under lock-protection
            que->EnQueue(task);
            taskCount = que->Size();
        }

        // The ownership of the task belongs to ReadyTaskQueue, and the task cannot be accessed any more.
        FFRT_LOGD("qos[%d] task[%llu], name[%s] entered q", level, gid, label.c_str());

        if (taskCount >= TASK_OVERRUN_THRESHOLD && taskCount % TASK_OVERRUN_ALARM_FREQ == 0) {
            FFRT_SYSEVENT_LOGW("qos [%d], task [%s] entered q, task count [%d] exceeds threshold.",
                level, label.c_str(), taskCount);
        }

        return taskCount == 1; // whether it's rising edge
    }

    TaskBase* PopTask() override
    {
        FFRT_PERF_TRACE_SCOPED_BY_GROUP(SCHED, STaskScheduler_PopTask, DEFAULT_CONFIG);
        TaskBase* task = nullptr;
        {
            // pop from global queue
            std::lock_guard<std::mutex> lock(*mtx);
            task = que->DeQueue();
        }

        if (task && task->type == ffrt_uv_task) {
            return GetUVTask(task);
        }
        return task;
    }
private:
    std::unique_ptr<FIFOQueue> que { nullptr };
};
}
#endif
