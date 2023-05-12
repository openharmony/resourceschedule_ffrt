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
#include "queue/itask.h"

namespace ffrt {
class SerialQueue : public NonCopyable {
public:
    explicit SerialQueue(const std::string& name) : name_(name) {}
    ~SerialQueue();

    ITask* Next();
    int PushTask(ITask* task, uint64_t upTime);
    int RemoveTask(const ITask* task);
    void Quit();

private:
    ffrt::mutex mutex_;
    ffrt::condition_variable cond_;
    bool isExit_ = false;
    std::string name_;
    std::map<uint64_t, std::list<ITask*>> whenMap_;
};
} // namespace ffrt

#endif // FFRT_SERIAL_QUEUE_H