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

#ifndef FFRT_TASK_RUNQUEUE_HPP
#define FFRT_TASK_RUNQUEUE_HPP

#include "tm/cpu_task.h"
#include "tm/uv_task.h"
#include <cassert>

namespace ffrt {
class RunQueue {
public:
    virtual ~RunQueue() = default;
    virtual void EnQueue(TaskBase* task) = 0;
    virtual TaskBase* DeQueue() = 0;
    virtual bool Empty() = 0;
    virtual int Size() = 0;
    virtual void SetQos(QoS &newQos) = 0;
};

class FIFOQueue : public RunQueue {
public:
    void EnQueue(TaskBase* task) override
    {
        list.PushBack(task->fq_we.node);
        auto curSize = size.load(std::memory_order_relaxed);
        size.store(curSize + 1, std::memory_order_relaxed);
    }

    TaskBase* DeQueue() override
    {
        if (list.Empty()) {
            return nullptr;
        }
        auto node = list.PopFront();
        if (node == nullptr) {
            return nullptr;
        }

        TaskBase* task = node->ContainerOf(&WaitEntry::node)->task;
        auto curSize = size.load(std::memory_order_relaxed);
        assert(curSize > 0);
        size.store(curSize - 1, std::memory_order_relaxed);
        return task;
    }

    bool Empty() override
    {
        return list.Empty();
    }

    int Size() override
    {
        return size.load(std::memory_order_relaxed);
    }
    void SetQos(QoS &newQos) override {}

private:
    LinkedList list;
    std::atomic<int> size = 0;
};
} // namespace ffrt

#endif
