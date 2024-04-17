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
#include <atomic>
#include <array>
#include "internal_inc/types.h"
#include "core/entity.h"
#include "eu/execute_unit.h"
#include "sync/sync.h"
#include "sched/task_scheduler.h"
#include "eu/worker_thread.h"
#include "tm/cpu_task.h"
#include "util/cb_func.h"
#include "dfx/bbox/bbox.h"

namespace ffrt {
class FFRTScheduler {
public:
    FFRTScheduler(const FFRTScheduler&) = delete;
    FFRTScheduler& operator=(const FFRTScheduler&) = delete;
    virtual ~FFRTScheduler()
    {
        for (int i = 0; i < QoS::Max(); i++) {
            SchedulerFactory::Recycle(fifoQue[i]);
        }
    }

    // 获取调度器的单例
    static FFRTScheduler* Instance();
    static void RegistInsCb(SingleInsCB<FFRTScheduler>::Instance &&cb);

#ifdef QOS_DEPENDENCY
    void onWait(const std::vector<VersionCtx*>& waitDatas, int64_t deadline)
    {
        for (auto data : waitDatas) {
            if (!data->childVersions.empty()) {
                auto waitVersion = data->childVersions.back();
                if (waitVersion->status != DataStatus::IDLE) { // 数据已经被生产出来
                    continue;
                }
                FFRT_LOGD("wait task=%p deadline=%ld", waitVersion->myProducer, deadline);
                updateTask(waitVersion->myProducer, deadline);
                updateVersion(waitVersion->preVersion, deadline);
            }
        }
    }
#endif

    TaskScheduler& GetScheduler(const QoS& qos)
    {
        return *fifoQue[static_cast<unsigned short>(qos)];
    }

#ifdef FFRT_IO_TASK_SCHEDULER
    virtual void PushTask(CPUEUTask* task);
#endif

    virtual bool InsertNode(LinkedList* node, const QoS qos);

    virtual bool RemoveNode(LinkedList* node, const QoS qos);

    virtual bool WakeupTask(CPUEUTask* task);

protected:
    FFRTScheduler()
    {
        TaskState::RegisterOps(TaskState::READY, std::bind(&FFRTScheduler::WakeupTask, this, std::placeholders::_1));
        for (int i = 0; i < QoS::Max(); i++) {
            fifoQue[i] = SchedulerFactory::Alloc();
        }
    }

private:
    std::array<TaskScheduler*, QoS::MaxNum()> fifoQue;
#ifdef QOS_DEPENDENCY
    void resetDeadline(CPUEUTask* task, int64_t deadline)
    {
        auto it = std::find_if(readyTasks.begin(), readyTasks.end(), [task](auto& p) { return p.second == task; });
        if (it == readyTasks.end()) {
            return;
        }
        auto node = readyTasks.extract(it);
        task->qos.deadline.relative += deadline - task->qos.deadline.absolute;
        task->qos.deadline.absolute = deadline;
        readyTasks.insert(std::move(node));
    }
    void updateTask(CPUEUTask* task, int64_t deadline)
    {
        if (task == nullptr) {
            return;
        }
        resetDeadline(task, deadline);
        onWait(task->ins, deadline);
        for (auto data : task->outs) {
            updateVersion(data, deadline);
        }
        updateChildTask(task, deadline);
    }
    void updateChildTask(CPUEUTask* task, int64_t deadline)
    {
        (void)task;
        (void)deadline;
    }
    void updateVersion(VersionCtx* data, int64_t deadline)
    {
        if (data == nullptr) {
            return;
        }
        updateTask(data->myProducer, deadline);
        for (auto task : data->consumers) {
            updateTask(task, deadline);
        }
        updateVersion(data->preVersion, deadline);
    }
#endif
};

class SFFRTScheduler : public FFRTScheduler {
public:
    static FFRTScheduler& Instance()
    {
        static SFFRTScheduler ins;
        return ins;
    }
private:
    SFFRTScheduler()
    {
    }
};
} // namespace ffrt
#endif
