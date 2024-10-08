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

#include "base_queue.h"
#include "dfx/log/ffrt_log_api.h"
#include "tm/queue_task.h"
#include "serial_queue.h"
#include "concurrent_queue.h"
#include "eventhandler_adapter_queue.h"
#include "eventhandler_interactive_queue.h"

namespace {
using CreateFunc = std::unique_ptr<ffrt::BaseQueue>(*)(const ffrt_queue_attr_t*);
const std::unordered_map<int, CreateFunc> CREATE_FUNC_MAP = {
    { ffrt_queue_serial, ffrt::CreateSerialQueue },
    { ffrt_queue_concurrent, ffrt::CreateConcurrentQueue },
    { ffrt_queue_eventhandler_interactive, ffrt::CreateEventHandlerInteractiveQueue },
    { ffrt_queue_eventhandler_adapter, ffrt::CreateEventHandlerAdapterQueue },
};
}

namespace ffrt {
// 0预留为非法值
std::atomic_uint32_t BaseQueue::queueId(1);
void BaseQueue::Stop()
{
    std::unique_lock lock(mutex_);
    isExit_ = true;

    ClearWhenMap();

    FFRT_LOGI("clear [queueId=%u] succ", queueId_);
}

void BaseQueue::Remove()
{
    std::unique_lock lock(mutex_);
    FFRT_COND_DO_ERR(isExit_, return, "cannot remove task, [queueId=%u] is exiting", queueId_);

    ClearWhenMap();

    FFRT_LOGD("cancel [queueId=%u] all tasks succ", queueId_);
}

int BaseQueue::Remove(const char* name)
{
    std::unique_lock lock(mutex_);
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot remove task, [queueId=%u] is exiting", queueId_);

    int removedCount = 0;
    for (auto iter = whenMap_.begin(); iter != whenMap_.end();) {
        if (iter->second->IsMatch(name)) {
            FFRT_LOGD("cancel task[%llu] %s succ", iter->second->gid, iter->second->label.c_str());
            iter->second->Notify();
            iter->second->Destroy();
            iter = whenMap_.erase(iter);
            removedCount++;
        } else {
            ++iter;
        }
    }

    return removedCount > 0 ? SUCC : FAILED;
}

int BaseQueue::Remove(const QueueTask* task)
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

bool BaseQueue::HasTask(const char* name)
{
    std::unique_lock lock(mutex_);
    auto iter = std::find_if(whenMap_.cbegin(), whenMap_.cend(),
        [name](const auto& pair) { return pair.second->IsMatch(name); });
    return iter != whenMap_.cend();
}

void BaseQueue::PrintMutexOwner()
{
    MutexOwnerType type = mutex_.GetOwnerType();
    if (type == MutexOwnerType::MUTEX_OWNER_TYPE_TASK) {
        FFRT_LOGI("In queue %u, task %llu owns the lock for %llu us.",
            queueId_, mutex_.GetOwnerId(), mutex_.GetDuration());
    } else {
        FFRT_LOGI("In queue %u, thread %llu owns the lock for %llu us.",
            queueId_, mutex_.GetOwnerId(), mutex_.GetDuration());
    }
}

void BaseQueue::ClearWhenMap()
{
    for (auto it = whenMap_.begin(); it != whenMap_.end(); it++) {
        if (it->second) {
            it->second->Notify();
            it->second->Destroy();
            it->second = nullptr;
        }
    }
    whenMap_.clear();
    cond_.NotifyOne();
}

std::unique_ptr<BaseQueue> CreateQueue(int queueType, const ffrt_queue_attr_t* attr)
{
    const auto iter = CREATE_FUNC_MAP.find(queueType);
    FFRT_COND_DO_ERR((iter == CREATE_FUNC_MAP.end()), return nullptr, "invalid queue type");

    return iter->second(attr);
}
} // namespace ffrt
