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

#include <mutex>

#include "core/entity.h"
#include "sched/task_runqueue.h"
#include "eu/worker_thread.h"
#include "sync/sync.h"
#include "sync/semaphore.h"
#include "dfx/trace/ffrt_trace.h"

namespace ffrt {

template <typename Sched>
class TaskScheduler {
public:
    virtual ~TaskScheduler() = default;

    TaskCtx* PickNextTask()
    {
        std::unique_lock lock(mutex);
        return static_cast<Sched*>(this)->PickNextTaskImpl();
    }

    bool WakeupTask(TaskCtx* task)
    {
        bool ret = false;
        {
            FFRT_READY_MARKER(task->gid);
            std::unique_lock lock(mutex);
            ret = static_cast<Sched*>(this)->WakeupTaskImpl(task);
        }
        return ret;
    }

    bool RQEmpty()
    {
        return static_cast<Sched*>(this)->RQEmptyImpl();
    }

    int RQSize()
    {
        return static_cast<Sched*>(this)->RQSizeImpl();
    }

private:
    fast_mutex mutex;
    semaphore sem;
};

class FIFOScheduler : public TaskScheduler<FIFOScheduler> {
    friend class TaskScheduler<FIFOScheduler>;

private:
    TaskCtx* PickNextTaskImpl()
    {
        TaskCtx* task = que.DeQueue();
        return task;
    }

    bool WakeupTaskImpl(TaskCtx* task)
    {
        que.EnQueue(task);
        return true;
    }

    bool RQEmptyImpl()
    {
        return que.Empty();
    }

    int RQSizeImpl()
    {
        return que.Size();
    }

    FIFOQueue que;
};

} // namespace ffrt

#endif
