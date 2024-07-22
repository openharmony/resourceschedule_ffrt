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
#ifndef FFRT_BASE_QUEUE_H
#define FFRT_BASE_QUEUE_H

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <memory>
#include "c/queue.h"
#include "cpp/condition_variable.h"
#include "internal_inc/non_copyable.h"
#include "queue_strategy.h"

namespace ffrt {
class QueueTask;
class Loop;

enum QueueAction {
    INACTIVE = -1, // queue is nullptr or serial queue is empty
    SUCC,
    FAILED,
    CONCURRENT, // concurrency less than max concurrency
};

class BaseQueue : public NonCopyable {
public:
    explicit BaseQueue() : queueId_(queueId++) {}
    virtual ~BaseQueue() = default;

    virtual int Push(QueueTask* task) = 0;
    virtual QueueTask* Pull() = 0;
    virtual bool GetActiveStatus() = 0;
    virtual int GetQueueType() const = 0;
    virtual void Remove();
    virtual int Remove(const char* name);
    virtual int Remove(const QueueTask* task);
    virtual void Stop();

    virtual bool IsOnLoop()
    {
        return false;
    }

    inline uint64_t GetMapSize()
    {
        std::unique_lock lock(mutex_);
        return whenMap_.size();
    }

    inline uint32_t GetQueueId() const
    {
        return queueId_;
    }

    bool HasTask(const char* name);

protected:
    inline uint64_t GetNow() const
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    void ClearWhenMap();

    const uint32_t queueId_;
    bool isExit_ { false };
    std::atomic_bool isActiveState_ { false };
    std::multimap<uint64_t, QueueTask*> whenMap_;
    QueueStrategy<QueueTask>::DequeFunc dequeFunc_ { nullptr };

    ffrt::mutex mutex_;
    ffrt::condition_variable cond_;

private:
    static std::atomic_uint32_t queueId;
};

std::unique_ptr<BaseQueue> CreateQueue(int queueType, const ffrt_queue_attr_t* attr);
} // namespace ffrt

#endif // FFRT_BASE_QUEUE_H