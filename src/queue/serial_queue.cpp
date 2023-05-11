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

#include "queue/serial_queue.h"
#include <chrono>
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
SerialQueue::~SerialQueue()
{
    Quit();
}

void SerialQueue::Quit()
{
    FFRT_LOGI("quit serial queue %s", name_.c_str());
    std::unique_lock lock(mutex_);
    if (isExit_) {
        return;
    }
    isExit_ = true;
    cond_.notify_all();

    for (auto it = whenMap_.begin(); it != whenMap_.end(); it++) {
        for (auto itList = it->second.begin(); itList != it->second.end(); itList++) {
            if (*itList != nullptr) {
                (*itList)->Notify();
                (*itList)->DecDeleteRef();
            }
        }
    }
    whenMap_.clear();
}

int SerialQueue::PushTask(ITask* task, uint64_t upTime)
{
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "failed to push task, task is nullptr", return -1);
    std::unique_lock lock(mutex_);
    whenMap_[upTime].emplace_back(task);
    if (upTime == whenMap_.begin()->first) {
        FFRT_LOGD("%s, Push Serial Task, Notify All", name_.c_str());
        cond_.notify_all();
    }
    return 0;
}

int SerialQueue::RemoveTask(const ITask* task)
{
    FFRT_COND_TRUE_DO_ERR((task == nullptr), "failed to remove task, task is nullptr", return -1);

    std::unique_lock lock(mutex_);
    for (auto it = whenMap_.begin(); it != whenMap_.end();) {
        for (auto itList = it->second.begin(); itList != it->second.end();) {
            if ((*itList) != task) {
                itList++;
                continue;
            }
            it->second.erase(itList++);
            // a task can be submitted only once through the C interface
            return 0;
        }

        if (it->second.empty()) {
            whenMap_.erase(it++);
        } else {
            it++;
        }
    }

    return 1;
}

ITask* SerialQueue::Next()
{
    std::unique_lock lock(mutex_);
    while (whenMap_.empty() && !isExit_) {
        FFRT_LOGD("Serial Queue %s is Empty, Begin to Wait", name_.c_str());
        cond_.wait(lock);
    }

    if (isExit_) {
        FFRT_LOGD("Serial Queue %s is Exit", name_.c_str());
        return nullptr;
    }

    auto nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    uint64_t now = static_cast<uint64_t>(nowUs.time_since_epoch().count());
    auto it = whenMap_.begin();
    if (now >= it->first) {
        if (it->second.empty()) {
            (void)whenMap_.erase(it);
            return nullptr;
        }
        auto nextTask = *it->second.begin();
        it->second.pop_front();
        if (it->second.empty()) {
            (void)whenMap_.erase(it);
        }
        return nextTask;
    } else {
        uint64_t diff = it->first - now;
        (void)cond_.wait_for(lock, std::chrono::microseconds(diff));
    }

    return nullptr;
}
} // namespace ffrt