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
#include "core/entity.h"
#include "sched/task_runqueue.h"
#include "eu/worker_thread.h"
#include "sync/sync.h"
#include "sync/semaphore.h"
#include "ffrt_trace.h"
#include "tm/cpu_task.h"

namespace ffrt {
class TaskScheduler {
public:
    virtual ~TaskScheduler() = default;

    CPUEUTask* PickNextTask()
    {
        return PickNextTaskImpl();
    }

    bool WakeupTask(CPUEUTask* task)
    {
        bool ret = false;
        {
            FFRT_READY_MARKER(task->gid);
            ret = WakeupTaskImpl(task);
        }
        return ret;
    }

    bool WakeupNode(LinkedList* node)
    {
        bool ret = false;
        {
            FFRT_EXECUTOR_TASK_READY_MARKER(reinterpret_cast<char*>(node) - offsetof(ffrt_executor_task_t, wq));
            ret = WakeupNodeImpl(node);
        }
        return ret;
    }

    bool RemoveNode(LinkedList* node)
    {
        bool ret = false;
        {
            ret = RemoveNodeImpl(node);
        }
        return ret;
    }

    bool RQEmpty()
    {
        return RQEmptyImpl();
    }

    int RQSize()
    {
        return RQSizeImpl();
    }

private:
    virtual CPUEUTask* PickNextTaskImpl() = 0;
    virtual bool WakeupNodeImpl(LinkedList* node) = 0;
    virtual bool RemoveNodeImpl(LinkedList* node) = 0;
    virtual bool WakeupTaskImpl(CPUEUTask* task) = 0;
    virtual bool RQEmptyImpl() = 0;
    virtual int RQSizeImpl() = 0;

    fast_mutex mutex;
    semaphore sem;
};

class SFIFOScheduler : public TaskScheduler {
private:
    CPUEUTask* PickNextTaskImpl() override
    {
        CPUEUTask* task = que.DeQueue();
        return task;
    }

    bool WakeupNodeImpl(LinkedList* node) override
    {
        que.EnQueueNode(node);
        return true;
    }

    bool RemoveNodeImpl(LinkedList* node) override
    {
        que.RmQueueNode(node);
        return true;
    }

    bool WakeupTaskImpl(CPUEUTask* task) override
    {
        que.EnQueue(task);
        return true;
    }

    bool RQEmptyImpl() override
    {
        return que.Empty();
    }

    int RQSizeImpl() override
    {
        return que.Size();
    }

    FIFOQueue que;
};

class SchedulerFactory {
public:
    using AllocCB = std::function<TaskScheduler *()>;
    using RecycleCB = std::function<void (TaskScheduler *)>;

    static SchedulerFactory &Instance()
    {
        static SchedulerFactory fac;
        return fac;
    }

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
