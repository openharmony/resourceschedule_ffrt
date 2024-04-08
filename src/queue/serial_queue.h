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
#ifndef FFRT_SERIAL_QUEUE_H
#define FFRT_SERIAL_QUEUE_H

#include <map>
#include <string>
#include <atomic>
#include <limits.h>
#include "cpp/condition_variable.h"
#include "internal_inc/non_copyable.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
enum QueueAction {
    INACTIVE = -1, // queue is null or serial queue is empty
    CONCURRENT,
    SUCC,
    FAILED,
};

class SerialTask;
class Loop;
class SerialQueue : public NonCopyable {
public:
    explicit SerialQueue(uint32_t queueId, const int maxConcurrency = 1, const ffrt_queue_type_t type = ffrt_queue_serial);
    ~SerialQueue();

    SerialTask* Pull();
    int Push(SerialTask* task);
    int Remove(const SerialTask* task);
    void Stop();

    bool SetLoop(Loop* loop);
    bool ClearLoop();
    bool IsOnLoop();

    int GetNextTimeOut();

    uint64_t GetMapSize();
    inline bool GetActiveStatus() const
    {
        bool status = isActiveState_.load();
        switch (queueType_) {
            case ffrt_queue_serial:
                status = isActiveState_.load();
                break;
            case ffrt_queue_concurrent:
                status = concurrency_.load();
                break;
            default: {
                FFRT_LOGE("Unsupport queue type=%d.", queueTYpe_);
                break;
            }
        }
        return status;
    }

private:
    SerialTask* DequeTaskBatch(const uint64_t now);
    SerialTask* DequeTaskPriorityWithGreedy(cons uint64_t now);

    int PushSerialTask(SerialTask* task);
    int PushConcurrentTask(SerialTask* task);
    int PushDelayTaskToTimer(SerialTask* task);

    SerialTask* PullSerialTask();
    SerialTask* PullConcurrentTask();

    const uint32_t queueId_;
    bool isExit_ = false;
    std::atomic_bool isActiveState_ = {0};
    std::multimap<uint64_t, SerialTask*> whenMap_;
    Loop* loop_ = nullptr;
    std::atomic_bool IsOnLoop_ = false;
    
    ffrt::mutex mutex_;
    ffrt::condition_variable cond_;

    int maxConcurrency_{1};
    std::atomic_int concurrency_{0};
    ffrt_queue_type_t queueType_ = ffrt_queue_serial;
};
} // namespace ffrt

#endif // FFRT_SERIAL_QUEUE_H