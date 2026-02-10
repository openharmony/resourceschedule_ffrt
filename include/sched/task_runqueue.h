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

#include "internal_inc/osal.h"
#include "tm/cpu_task.h"

namespace ffrt {
class FIFOQueue {
public:
    void EnQueue(TaskBase* task)
    {
        list.PushBack(task->node);
        auto curSize = size.load(std::memory_order_relaxed);
        size.store(curSize + 1, std::memory_order_relaxed);
    }

    void EnQueueBatch(TaskBase* first, TaskBase* last, size_t cnt)
    {
        list.PushBack(first->node, last->node);
        auto curSize = size.load(std::memory_order_relaxed);
        size.store(curSize + static_cast<int>(cnt), std::memory_order_relaxed);
    }

    TaskBase* DeQueue()
    {
        if (list.Empty()) {
            if (unlikely(size.load(std::memory_order_relaxed)) != 0) {
                FFRT_LOGE("size not match, current size %d, reset to 0.", size.load(std::memory_order_relaxed));
                size.store(0, std::memory_order_relaxed);
            }
            return nullptr;
        }
        auto node = list.PopFront();
        if (node == nullptr) {
            FFRT_LOGE("failed to pop node, current size %d.", size.load(std::memory_order_relaxed));
            return nullptr;
        }

        TaskBase* task = node->ContainerOf(&TaskBase::node);
        auto curSize = size.load(std::memory_order_relaxed);
        size.store(curSize - 1, std::memory_order_relaxed);
        return task;
    }

    bool Empty()
    {
        return list.Empty();
    }

    int Size()
    {
        return size.load(std::memory_order_relaxed);
    }

private:
    LinkedList list;
    std::atomic<int> size = 0;
};
} // namespace ffrt

#endif
