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
#include "serial_task.h"
#include "serial_handler.h"
#include "eu/loop.h"

namespace {
inline uint64_t GetNow()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

namespace ffrt {
SerialQueue::SerialQueue(uint32_t queueId, const int maxConcurrency, const ffrt_queue_type_t type)
    : queueId_(queueId), maxConcurrency_(maxConcurrency), queueType_(type) {}

SerialQueue::~SerialQueue()
{
    FFRT_LOGI("destruct queueId=%u leave", queueId_);
}

bool SerialQueue::SetLoop(Loop* loop)
{
    if (loop == nullptr || loop_ != nullptr) {
        FFRT_LOGE("queueId %s should bind to loop invalid", queueId_);
        return false;
    }

    loop_ = loop;
    isOnLoop_.store(true);
    return true;
}

bool SerialQueue::ClearLoop()
{
    if (loop_ == nullptr) {
        return false;
    }

    loop_ = nullptr;
    return true;
}

bool SerialQueue::IsOnLoop()
{
    return isOnLoop_.load();
}

int SerialQueue::GetNextTimeout()
{
    std::unique_lock lock(mutex_);
    if (whenMap_.empty()) {
        return -1;
    }
    uint64_t now = GetNow();
    if (now >= whenMap_.begin()->first) {
        return 0;
    }
    uint64_t diff = whenMap_.begin()->first - now;
    uint64_t timeout = (diff - 1) / 1000 + 1; // us->ms
    return timeout > INT_MAX ? INT_MAX : static_cast<int>(timeout);
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
    if (loop_ == nullptr) {
        cond_.notify_one();
    }

    FFRT_LOGI("clear [queueId=%u] succ", queueId_);
}

int SerialQueue::PushSerialTask(SerialTask* task)
{
    if (!isActiveState_.load()) {
        isActiveState_.store(true);
        return INACTIVE;
    }

    whenMap_.insert({task->GetUptime(), task});
    if (task == whenMap_.begin()->second) {
        cond_.notify_all();
    }

    return SUCC;
}

static void DelayTaskCb(void* task)
{
    static_cast<SerialTask*>(task)->Execute();
}

int SerialQueue::PushDelayTaskToTimer(SerialTask* task)
{
    uint64_t delayMs = (task->GetDelay() - 1) / 1000 + 1;
    int timeout = delayMs > INT_MAX ? INT_MAX : delayMs;
    if (loop_->TimerStart(timeout, task, DelayTaskCb, false) < 0) {
        FFRT_LOGE("push delay queue task to timer fail");
        return FAILED;
    }
    return SUCC;
}

int SerialQueue::PushConcurrentTask(SerialTask* task)
{
    if (loop_ != nullptr) {
        if (task->GetDelay() == 0) {
            whenMap_.insert({task->GetUptime(), task});
            loop_->WakeUp();
            return SUCC;
        }
        return PushDelayTaskToTimer(task);
    }
    FFRT_COND_DO_ERR(IsOnLoop(), return FAILED, "cannot push task, [queueId=%u] loop empty", queueId_);

    if (concurrency_.load() < maxConcurrency_) {
        int oldValue = concurrency_.fetch_add(1);
        FFRT_LOGD("task [gid=%llu] concurrency[%u] + 1 [queueId=%u]", task->gid, oldValue, queueId_);

        if (task->GetDelay() > 0) {
            whenMap_.insert({task->GetUptime(), task});
        }

        return CONCURRENT;
    }

    whenMap_.insert({task->GetUptime(), task});
    if (task == whenMap_.begin()->second) {
        cond_.notify_all();
    }

    return SUCC;
}

int SerialQueue::Push(SerialTask* task)
{
    std::unique_lock lock(mutex_);
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot push task, [queueId=%u] is exiting", queueId_);
    int status = 0;
    switch (queueType_) {
        case ffrt_queue_serial:
            status = PushSerialTask(task);
            break;
        case ffrt_queue_concurrent:
            status = PushConcurrentTask(task);
            break;
        default: {
            FFRT_LOGE("Unsupport queue type=%d.", queueType_);
            break;
        }
    }

    return status;
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

SerialTask* SerialQueue::PullSerialTask()
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
    return DequeTaskBatch(now);
}

SerialTask* SerialQueue::PullConcurrentTask()
{
    std::unique_lock lock(mutex_);
    // wait for delay task
    uint64_t now = GetNow();
    if (loop_ != nullptr) {
        if (!whenMap_.empty() && now >= whenMap_.begin()->first && !isExit_) {
            return DequeTaskPriorityWithGreedy(now);
        }
        return nullptr;
    }

    while (!whenMap_.empty() && now < whenMap_.begin()->first && !isExit_) {
        uint64_t diff = whenMap_.begin()->first - now;
        FFRT_LOGD("[queueId=%u] stuck in %llu us wait", queueId_, diff);
        cond_.wait_for(lock, std::chrono::microseconds(diff));
        FFRT_LOGD("[queueId=%u] wakeup from wait", queueId_);
        now = GetNow();
    }

    // abort dequeue in abnormal scenarios
    if (whenMap_.empty()) {
        uint8_t oldValue = concurrency_.fetch_sub(1); // 取不到后继的task，当前这个task正式退出
        FFRT_LOGD("concurrency[%u] - 1 [queueId=%u] switch into inactive", oldValue, queueId_);
        return nullptr;
    }
    FFRT_COND_DO_ERR(isExit_, return nullptr, "cannot pull task, [queueId=%u] is exiting", queueId_);

    // dequeue next expired task by priority
    return DequeTaskPriorityWithGreedy(now);
}

SerialTask* SerialQueue::Pull()
{
    SerialTask* task = nullptr;
    switch (queueType_) {
        case ffrt_queue_serial:
            task = PullSerialTask();
            break;
        case ffrt_queue_concurrent:
            task = PullConcurrentTask();
            break;
        default: {
            FFRT_LOGE("Unsupport queue type=%d.", queueType_);
            break;
        }
    }
    return task;
}

SerialTask* SerialQueue::DequeTaskBatch(const uint64_t now)
{
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

SerialTask* SerialQueue::DequeTaskPriorityWithGreedy(const uint64_t now)
{
    // dequeue next expired task by priority
    auto ite_target = whenMap_.begin();
    for (auto ite = whenMap_.begin(); ite != whenMap_.end() && ite->first < now; ite++) {
        if (ite_target->second->GetPriority() == ffrt_queue_priority_immediate) {
            break;
        }
        if (ite->second->GetPriority() < ite_target->second->GetPriority()) {
            ite_target = ite;
        }
    }

    SerialTask* head = ite_target->second;
    whenMap_.erase(ite_target);

    FFRT_LOGD("dequeue [gid=%llu], %u other tasks in [queueId=%u] ",
        head->gid, whenMap_.size(), queueId_);
    return head;
}

uint64_t SerialQueue::GetMapSize()
{
    std::unique_lock lock(mutex_);
    return whenMap_.size();
}

bool SerialQueue::GetActiveStatus() const
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
            FFRT_LOGE("Unsupport queue type=%d.", queueType_);
            break;
        }
    }
    return status;
}
} // namespace ffrt
