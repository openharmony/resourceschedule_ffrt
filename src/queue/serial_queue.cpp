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
#include "dfx/log/ffrt_log_api.h"
#include "tm/queue_task.h"

namespace ffrt {
SerialQueue::~SerialQueue()
{
    FFRT_LOGI("destruct serial queueId=%u leave", queueId_);
}

int SerialQueue::Push(QueueTask* task)
{
    std::unique_lock lock(mutex_);
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot push task, [queueId=%u] is exiting", queueId_);

    if (!isActiveState_.load()) {
        isActiveState_.store(true);
        return INACTIVE;
    }

    whenMap_.insert({task->GetUptime(), task});
    if (task == whenMap_.begin()->second) {
        cond_.NotifyOne();
    }

    return SUCC;
}

QueueTask* SerialQueue::Pull()
{
    std::unique_lock lock(mutex_);
    // wait for delay task
    uint64_t now = GetNow();
    while (!whenMap_.empty() && now < whenMap_.begin()->first && !isExit_) {
        uint64_t diff = whenMap_.begin()->first - now;
        FFRT_LOGD("[queueId=%u] stuck in %llu us wait", queueId_, diff);
        cond_.WaitFor(lock, std::chrono::microseconds(diff));
        FFRT_LOGD("[queueId=%u] wakeup from wait", queueId_);
        now = GetNow();
    }

    // abort dequeue in abnormal scenarios
    if (whenMap_.empty()) {
        FFRT_LOGD("[queueId=%u] switch into inactive", queueId_);
        isActiveState_.store(false);
        return nullptr;
    }
    FFRT_COND_DO_ERR(isExit_, return nullptr, "cannot pull task, [queueId=%u] is exiting", queueId_);

    // dequeue due tasks in batch
    return dequeFunc_(queueId_, now, whenMap_, nullptr);
}

std::unique_ptr<BaseQueue> CreateSerialQueue(const ffrt_queue_attr_t* attr)
{
    (void)attr;
    return std::make_unique<SerialQueue>();
}
} // namespace ffrt
