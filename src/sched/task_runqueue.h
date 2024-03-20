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


#include "c/executor_task.h"
#include "tm/cpu_task.h"

namespace ffrt {
class RunQueue {
public:
    virtual ~RunQueue() = default;

    void EnQueue(CPUEUTask* task)
    {
        EnQueueImpl(task);
    }

    CPUEUTask* DeQueue()
    {
        return DeQueueImpl();
    }

    void EnQueueNode(LinkedList* node)
    {
        EnQueueNodeImpl(node);
    }

    void RmQueueNode(LinkedList* node)
    {
        RmQueueNodeImpl(node);
    }

    bool Empty()
    {
        return EmptyImpl();
    }

    int Size()
    {
        return SizeImpl();
    }

protected:
    LinkedList list;
    int size = 0;

private:
    virtual void EnQueueImpl(CPUEUTask* task) = 0;
    virtual CPUEUTask* DeQueueImpl() = 0;
    virtual void EnQueueNodeImpl(LinkedList* node) = 0;
    virtual void RmQueueNodeImpl(LinkedList* node) = 0;
    virtual bool EmptyImpl() = 0;
    virtual int SizeImpl() = 0;
};

class FIFOQueue : public RunQueue {
private:
    void EnQueueImpl(CPUEUTask* task) override
    {
        auto entry = &task->fq_we;
        list.PushBack(entry->node);
        size++;
    }

    CPUEUTask* DeQueueImpl() override
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
        if (w->type != ffrt_normal_task && w->type != ffrt_serial_task) {
            w->wq[0] = &w->wq;
            w->wq[1] = &w->wq;
            size--;
            return reinterpret_cast<CPUEUTask *>(w);
        }

        auto entry = node->ContainerOf(&WaitEntry::node);
        CPUEUTask* tsk = entry->task;

        size--;
        return tsk;
    }

    void EnQueueNodeImpl(LinkedList* node) override
    {
        list.PushBack(*node);
        size++;
    }

    void RmQueueNodeImpl(LinkedList* node) override
    {
        list.Delete(*node);
        size--;
    }

    bool EmptyImpl() override
    {
        return list.Empty();
    }

    int SizeImpl() override
    {
        return size;
    }
};
} // namespace ffrt

#endif
