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
#include "cpp/condition_variable.h"
#include "internal_inc/non_copyable.h"
#include "itask.h"

namespace ffrt {
enum QueueAction {
    INACTIVE = -1;
    SUCC,
    FAILED,
};

class SerialTask;
class SerialQueue : public NonCopyable {
public:
    explicit SerialQueue(uint32_t queueId);
    ~SerialQueue();

    SerialTask* Pull();
    int Push(SerialTask* task);
    int Remove(const SerialTask* task);
    void Stop();

    uint64_t GetMapSize();
    inline bool GetActiveState() const
    {
        return isActiveState_.load();
    }

private:
    const uint32_t queueId_;
    bool isExit_ = false;
    std::atomic_bool isActiveState_ = {0};
    std::multimap<uint64_t, SerialTask*> whenMap_;

    ffrt::mutex mutex_;
    ffrt::condition_variable cond_;
};
} // namespace ffrt

#endif // FFRT_SERIAL_QUEUE_H