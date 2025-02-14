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
#include "ffrt_trace.h"
#include "tm/cpu_task.h"
#include "dfx/perf/ffrt_perf.h"

namespace ffrt {
class TaskScheduler {
public:
    TaskScheduler(RunQueue* q) : que(q) {}
    ~TaskScheduler()
    {
        if (que != nullptr) {
            delete que;
        }
    }

    CPUEUTask* PickNextTask()
    {
        CPUEUTask* task = que->DeQueue();
        return task;
    }

    bool WakeupTask(CPUEUTask* task)
    {
        bool ret = false;
        {
            que->EnQueue(task);
            ret = true;
        }
        return ret;
    }

    bool WakeupNode(LinkedList* node)
    {
        bool ret = false;
        {
            que->EnQueueNode(node);
            ret = true;
        }
        return ret;
    }

    bool RemoveNode(LinkedList* node)
    {
        bool ret = false;
        {
            que->RmQueueNode(node);
            ret = true;
        }
        return ret;
    }

    bool RQEmpty()
    {
        return que->Empty();
    }

    int RQSize()
    {
        return que->Size();
    }

    void SetQos(QoS &q)
    {
        que->SetQos(q);
    }

    int qos {0};
private:
    RunQueue *que;
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
