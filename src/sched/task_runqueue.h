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


#include "core/task_ctx.h"

namespace ffrt {
template <typename Derived>
class RunQueue {
public:
    virtual ~RunQueue() = default;

    void EnQueue(TaskCtx* task)
    {
        static_cast<Derived*>(this)->EnQueueImpl(task);
    }

    void EnQueueNode(LinkedList* node)
    {
        static_cast<Derived*>(this)->EnQueueNodeImpl(node);
    }

    void RmQueueNode(LinkedList* node)
    {
        static_cast<Derived*>(this)->RmQueueNodeImpl(node);
    }

    TaskCtx* DeQueue()
    {
        return static_cast<Derived*>(this)->DeQueueImpl();
    }

    bool Empty()
    {
        return static_cast<Derived*>(this)->EmptyImpl();
    }

    int Size()
    {
        return static_cast<Derived*>(this)->SizeImpl();
    }
};

class FIFOQueue : public RunQueue<FIFOQueue> {
    friend class RunQueue<FIFOQueue>;

private:
    void EnQueueImpl(TaskCtx* task)
    {
        auto entry = &task->fq_we;
        list.PushBack(entry->node);
        size++;
    }

    TaskCtx* DeQueueImpl()
    {
        if (list.Empty()) {
            return nullptr;
        }
        auto node = list.PopFront();
        ffrt_executor_task_t* w = (ffrt_executor_task_t *) ((char *)(node) - offsetof(ffrt_executor_task_t, wq));
        if (w->type != 0) {
            w->wq[0] = &w->wq;
            w->wq[1] = &w->wq;
            size--;
            return (TaskCtx *)w;
        }

        auto entry = node->ContainerOf(&WaitEntry::node);
        TaskCtx* tsk = entry->task;

        size--;
        return tsk;
    }

    void EnQueueNodeImpl(LinkedList* node)
    {
        list.PushBack(*node);
        size++;
    }

    void RmQueueNodeImpl(LinkedList* node)
    {
        list.Delete(*node);
        size--;
    }

    bool EmptyImpl()
    {
        return list.Empty();
    }

    int SizeImpl()
    {
        return size;
    }

    LinkedList list;
    int size = 0;
};
} // namespace ffrt

#endif
