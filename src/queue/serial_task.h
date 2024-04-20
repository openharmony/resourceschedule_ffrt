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
#ifndef FFRT_SERIAL_TASK_H
#define FFRT_SERIAL_TASK_H

#include <atomic>
#include "queue_attr_private.h"
#include "ihandler.h"
#include "util/task_deleter.h"
#include "tm/cpu_task.h"

#define GetSerialTaskByFuncStorageOffset(f)                                                                     \
    (reinterpret_cast<SerialTask *>(static_cast<uintptr_t>(static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) - \
        (reinterpret_cast<size_t>(&((reinterpret_cast<SerialTask *>(0))->func_storage))))))

namespace ffrt {
class SerialTask : public CoTask {
public:
    explicit SerialTask(IHandler* handler, const task_attr_private* attr = nullptr);
    ~SerialTask();

    void Destroy();
    void Wait();
    void Notify();
    void Execute() override;

    inline int GetQos() const
    {
        return qos_;
    }

    inline void SetQos(int qos)
    {
        qos_ = qos;
    }

    inline uint64_t GetDelay() const
    {
        return delay_;
    }

    inline uint64_t GetUptime() const
    {
        return uptime_;
    }

    inline IHandler* GetHandler() const
    {
        return handler_;
    }

    inline uint32_t GetQueueId() const
    {
        return handler_->GetQueueId();
    }

    inline bool GetFinishStatus() const
    {
        return isFinished_.load();
    }

    inline SerialTask* GetNextTask() const
    {
        return nextTask_;
    }

    inline void SetNextTask(SerialTask* task)
    {
        nextTask_ = task;
    }

    inline void SetPriority(const ffrt_queue_priority_t prio)
    {
        prio_ = prio;
    }

    inline ffrt_queue_priority_t GetPriority()
    {
        return prio_;
    }

    inline void SetName(const std::string name)
    {
        name_ = name;
    }

    inline std::string GetName()
    {
        return name_;
    }

    uint8_t func_storage[ffrt_auto_managed_function_storage_size];

private:
    void FreeMem() override;
    uint64_t uptime_;
    IHandler* handler_;
    uint64_t delay_ = 0;
    int qos_ = qos_inherit;

    SerialTask* nextTask_ = nullptr;
    std::atomic_bool isFinished_ = {false};
    bool onWait_ = {false};

    std::mutex mutex_;
    std::condition_variable cond_;

    ffrt_queue_priority_t prio_ = ffrt_queue_priority_low;
    std::string name_;
};
} // namespace ffrt

#endif // FFRT_SERIAL_TASK_H
