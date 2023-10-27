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

#include <list>
#include <map>
#include <string>
#include "cpp/condition_variable.h"
#include "internal_inc/non_copyable.h"
#include "itask.h"

namespace ffrt {
class SerialQueue : public NonCopyable {
public:
    SerialQueue(const uint32_t qid, const std::string& name) : qid_(qid), name_(name) {}
    ~SerialQueue();

    inline uint32_t GetMapSize() const
    {
        return mapSize_.load();
    }

    ITask* Next();
    int PushTask(ITask* task, uint64_t upTime);
    int RemoveTask(const ITask* task);
    void Quit();

private:
    ffrt::mutex mutex_;
    ffrt::condition_variable cond_;
    bool isExit_ = false;
    const uint32_t qid_;
    std::string name_;
    std::atomic_uint32_t mapSize_ = {0};
    std::map<uint64_t, std::list<ITask*>> whenMap_;
};
} // namespace ffrt

#endif // FFRT_SERIAL_QUEUE_H