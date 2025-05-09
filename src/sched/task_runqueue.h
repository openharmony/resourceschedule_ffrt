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

namespace ffrt {
class RunQueue {
public:
    virtual ~RunQueue() = default;
    virtual void EnQueue(CPUEUTask* task) = 0;
    virtual CPUEUTask* DeQueue() = 0;
    virtual void EnQueueNode(LinkedList* node) = 0;
    virtual void RmQueueNode(LinkedList* node) = 0;
    virtual bool Empty() = 0;
    virtual int Size() = 0;
    virtual void SetQos(QoS &newQos) = 0;

protected:
    LinkedList list;
    // we defined size atomic to be
    // able to safely read it without
    // introducing a data-race or acquiring
    // a lock.
    std::atomic_int size = 0;
};

class FIFOQueue : public RunQueue {
public:
    void EnQueue(CPUEUTask* task) override
    {
        auto entry = &task->fq_we;
        list.PushBack(entry->node);
        size.fetch_add(1, std::memory_order_relaxed);
    }

    CPUEUTask* DeQueue() override
    {
        if (list.Empty()) {
            return nullptr;
        }
        auto node = list.PopFront();
        if (node == nullptr) {
            return nullptr;
        }
        ffrt_executor_task_t* w = reinterpret_cast<ffrt_executor_task_t *>(reinterpret_cast<char *>(node) -
            offsetof(ffrt_executor_task_t, wq));
        if (w->type > ffrt_invalid_task || w->type == ffrt_uv_task) {
            w->wq[0] = &w->wq;
            w->wq[1] = &w->wq;
            size.fetch_sub(1, std::memory_order_relaxed);
            return reinterpret_cast<CPUEUTask *>(w);
        }

        auto entry = node->ContainerOf(&WaitEntry::node);
        CPUEUTask* tsk = entry->task;

        size.fetch_sub(1, std::memory_order_relaxed);
        return tsk;
    }

    void EnQueueNode(LinkedList* node) override
    {
        list.PushBack(*node);
        size.fetch_add(1, std::memory_order_relaxed);
    }

    void RmQueueNode(LinkedList* node) override
    {
        list.Delete(*node);
        size.fetch_sub(1, std::memory_order_relaxed);
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
};
} // namespace ffrt

#endif
