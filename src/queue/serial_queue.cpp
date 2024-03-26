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

#include "serial_queue.h"

#include <chrono>
#include "dfx/log/ffrt_log_api.h"
#include "serial_task.h"
#include "serial_handler.h"

namespace {
inline uint64_t GetNow()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

namespace ffrt {
SerialQueue::SerialQueue(uint32_t queueId) : queueId_(queueId) {}

SerialQueue::~SerialQueue()
{
    FFRT_LOGI("destruct queueId=%u leave", queueId_);
}

void SerialQueue::Stop()
{
    std::unique_lock lock(mutex_);
    isExit_ = true;

    for (auto it = whenMap_.begin(); it != whenMap_.end(); it++) {
        if (it->second) {
            it->second->Notify();
            it->second->Destroy();
        }
    }
    whenMap_.clear();
    cond_.notify_one();

    FFRT_LOGI("clear [queueId=%u] succ", queueId_);
}

int SerialQueue::Push(SerialTask* task)
{
    std::unique_lock lock(mutex_);
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot push task, [queueId=%u] is exiting", queueId_);

    if (!isActiveState_.load()) {
        isActiveState_.store(true);
        return INACTIVE;
    }

    whenMap_.insert({task->GetUptime(), task});
    if (task == whenMap_.begin()->second) {
        cond_.notify_one();
    }

    return SUCC;
}

int SerialQueue::Remove(const SerialTask* task)
{
    std::unique_lock lock(mutex_);
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot remove task, [queueId=%u] is exiting", queueId_);

    auto range = whenMap_.equal_range(task->GetUptime());
    for (auto it = range.first; it != range.second; it++) {
        if (it->second == task) {
            whenMap_.erase(it);
            return SUCC;
        }
    }

    return FAILED;
}

SerialTask* SerialQueue::Pull()
{
    std::unique_lock lock(mutex_);

    // wait for delay task
    uint64_t now = GetNow();
    while (!whenMap_.empty() && now < whenMap_.begin()->first && !isExit_) {
        uint64_t diff = whenMap_.begin()->first - now;
        FFRT_LOGD("[queueId=%u] stuck in %llu us wait", queueId_, diff);
        cond_.wait_for(lock, std::chrono::microseconds(diff));
        FFRT_LOGD("[queueId=%u] wakeup from wait", queueId_);
        now = GetNow();
    }

    // abort dequeue in abnormal scenarios
    if (whenMap_.empty()) {
        isActiveState_.store(false);
        FFRT_LOGD("[queueId=%u] switch into inactive", queueId_);
        return nullptr;
    }
    FFRT_COND_DO_ERR(isExit_, return nullptr, "cannot pull task, [queueId=%u] is exiting", queueId_);

    // dequeue due tasks in batch
    SerialTask* head = whenMap_.begin()->second;
    whenMap_.erase(whenMap_.begin());

    SerialTask* node = head;
    while (!whenMap_.empty() && whenMap_.begin()->first < now) {
        auto next = whenMap_.begin()->second;
        if (next->GetQos() != head->GetQos()) {
            break;
        }
        node->SetNextTask(next);
        whenMap_.erase(whenMap_.begin());
        node = next;
    }

    FFRT_LOGD("dequeue [gid=%llu -> gid=%llu], %u other tasks in [queueId=%u] ",
        head->gid, node->gid, whenMap_.size(), queueId_);
    return head;
}

uint64_t SerialQueue::GetMapSize()
{
    std::unique_lock lock(mutex_);
    return whenMap_.size();
}
} // namespace ffrt
