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

#ifndef FFRT_SCHEDULER_HPP
#define FFRT_SCHEDULER_HPP
#include <list>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <array>
#include "eu/execute_unit.h"
#include "sync/sync.h"
#include "sched/task_scheduler.h"
#include "tm/cpu_task.h"
#include "util/cb_func.h"
#include "dfx/bbox/bbox.h"

namespace ffrt {
class Scheduler {
public:
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    virtual ~Scheduler()
    {
        tearDown = true;
        for (int i = 0; i < QoS::Max(); i++) {
            SchedulerFactory::Recycle(taskSchedulers[i]);
        }
    }

    // 获取调度器的单例
    static inline Scheduler* Instance()
    {
        if unlikely(schedulerIns_ == nullptr) {
            CreateInstance();
        }
        return schedulerIns_;
    }
    static inline Scheduler* schedulerIns_ = nullptr;

    inline TaskScheduler& GetScheduler(const QoS& qos)
    {
        return *taskSchedulers[static_cast<unsigned short>(qos)];
    }

    bool PushTask(TaskBase* task, bool rtb = true)
    {
        if (!tearDown && task) {
            return taskSchedulers[task->qos_]->PushTask(task, rtb);
        }
        return false;
    }

    TaskBase* PopTask(const QoS& qos)
    {
        if (tearDown) {
            return nullptr;
        }

        TaskBase* task = nullptr;
        task = taskSchedulers[qos]->PopTask();
        if (task) {
            task->Pop();
        }
        return task;
    }

    inline uint64_t GetTotalTaskCnt(const QoS& qos)
    {
        return taskSchedulers[static_cast<unsigned short>(qos)]->GetTotalTaskCnt();
    }

    inline uint64_t GetGlobalTaskCnt(const QoS& qos)
    {
        return taskSchedulers[static_cast<unsigned short>(qos)]->GetGlobalTaskCnt();
    }

    bool CancelUVWork(ffrt_executor_task_t* uvWork, int qos);

    bool CheckUVTaskConcurrency(ffrt_executor_task_t* task, const QoS& qos);
    ffrt_executor_task_t* PickWaitingUVTask(const QoS& qos);

    std::atomic_bool tearDown { false };

private:
    std::array<TaskScheduler*, QoS::MaxNum()> taskSchedulers;
    Scheduler()
    {
        for (int i = 0; i < QoS::Max(); i++) {
            taskSchedulers[i] = SchedulerFactory::Alloc();
            QoS qos = QoS(i);
            GetScheduler(i).SetQos(qos);
        }
    }
    static FFRT_NOINLINE void CreateInstance();
};
} // namespace ffrt
#endif
