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

#include "concurrent_queue.h"
#include <climits>
#include "dfx/log/ffrt_log_api.h"
#include "tm/queue_task.h"
#include "eu/loop.h"

namespace ffrt {
static void DelayTaskCb(void* task)
{
    static_cast<QueueTask*>(task)->Execute();
}

ConcurrentQueue::~ConcurrentQueue()
{
    FFRT_LOGI("destruct concurrent queueId=%u leave", queueId_);
}

int ConcurrentQueue::Push(QueueTask* task)
{
    std::unique_lock lock(mutex_);
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot push task, [queueId=%u] is exiting", queueId_);
    if (task->GetPriority() > ffrt_queue_priority_idle) {
        task->SetPriority(ffrt_queue_priority_low);
    }

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

QueueTask* ConcurrentQueue::Pull()
{
    std::unique_lock lock(mutex_);
    // wait for delay task
    uint64_t now = GetNow();
    if (loop_ != nullptr) {
        if (!whenMap_.empty() && now >= whenMap_.begin()->first && !isExit_) {
            return dequeFunc_(queueId_, now, whenMap_, nullptr);
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
        uint32_t queueId = queueId_;
        uint8_t oldValue = concurrency_.fetch_sub(1); // 取不到后继的task，当前这个task正式退出
        FFRT_LOGD("concurrency[%u] - 1 [queueId=%u] switch into inactive", oldValue, queueId);
        return nullptr;
    }
    FFRT_COND_DO_ERR(isExit_, return nullptr, "cannot pull task, [queueId=%u] is exiting", queueId_);

    // dequeue next expired task by priority
    return dequeFunc_(queueId_, now, whenMap_, nullptr);
}

void ConcurrentQueue::Stop()
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
        cond_.notify_all();
    }

    FFRT_LOGI("clear [queueId=%u] succ", queueId_);
}

bool ConcurrentQueue::SetLoop(Loop* loop)
{
    if (loop == nullptr || loop_ != nullptr) {
        FFRT_LOGE("queueId %s should bind to loop invalid", queueId_);
        return false;
    }

    loop_ = loop;
    isOnLoop_.store(true);
    return true;
}

int ConcurrentQueue::PushDelayTaskToTimer(QueueTask* task)
{
    uint64_t delayMs = (task->GetDelay() - 1) / 1000 + 1;
    int timeout = delayMs > INT_MAX ? INT_MAX : delayMs;
    if (loop_->TimerStart(timeout, task, DelayTaskCb, false) < 0) {
        FFRT_LOGE("push delay queue task to timer fail");
        return FAILED;
    }
    return SUCC;
}

std::unique_ptr<BaseQueue> CreateConcurrentQueue(uint32_t queueId, const ffrt_queue_attr_t* attr)
{
    int maxConcurrency = ffrt_queue_attr_get_max_concurrency(attr) <= 0 ? 1 : ffrt_queue_attr_get_max_concurrency(attr);
    return std::make_unique<ConcurrentQueue>(queueId, maxConcurrency);
}
} // namespace ffrt