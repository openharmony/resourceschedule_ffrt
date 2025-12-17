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

#include <unordered_map>
#include "dfx/log/ffrt_log_api.h"
#include "tm/queue_task.h"
#include "queue/serial_queue.h"
#include "concurrent_queue.h"
#include "eventhandler_adapter_queue.h"
#include "eventhandler_interactive_queue.h"
#include "util/time_format.h"
#include "queue/base_queue.h"

namespace {
constexpr uint32_t QUEUE_NAME_SIZE_MAX = 128;
// 0预留为非法值
std::atomic_uint32_t g_queueId(1);
using CreateFunc = std::unique_ptr<ffrt::BaseQueue>(*)(const ffrt_queue_attr_t*, const char* name);
const std::unordered_map<int, CreateFunc> CREATE_FUNC_MAP = {
    { ffrt_queue_serial, ffrt::CreateSerialQueue },
    { ffrt_queue_concurrent, ffrt::CreateConcurrentQueue },
    { ffrt_queue_eventhandler_interactive, ffrt::CreateEventHandlerInteractiveQueue },
    { ffrt_queue_eventhandler_adapter, ffrt::CreateEventHandlerAdapterQueue },
};

int ClearWhenMap(std::multimap<uint64_t, ffrt::QueueTask*>& whenMap, ffrt::condition_variable& cond)
{
    for (auto it = whenMap.begin(); it != whenMap.end(); it++) {
        if (it->second) {
            it->second->Cancel();
            it->second = nullptr;
        }
    }
    int mapSize = static_cast<int>(whenMap.size());
    whenMap.clear();
    cond.notify_one();
    return mapSize;
}
}

namespace ffrt {
BaseQueue::BaseQueue(const char* name) : queueId_(g_queueId++)
{
    headTaskVec_.resize(1);
    if (name != nullptr && std::string(name).size() <= QUEUE_NAME_SIZE_MAX) {
        name_ = "sq_" + std::string(name) + "_" + std::to_string(queueId_);
    } else {
        name_ = "sq_unnamed_" + std::to_string(queueId_);
        FFRT_LOGW("failed to set [queueId=%u] name due to invalid name or length.", queueId_);
    }
}

void BaseQueue::Stop()
{
    std::lock_guard lock(mutex_);
    Stop(whenMap_);
    FFRT_LOGI("clear [queueId=%u] succ", queueId_);
}

void BaseQueue::Stop(std::multimap<uint64_t, QueueTask*>& whenMap)
{
    isExit_ = true;
    ClearWhenMap(whenMap, cond_);
}

int BaseQueue::Remove()
{
    std::lock_guard lock(mutex_);
    return Remove(whenMap_);
}

int BaseQueue::Remove(std::multimap<uint64_t, QueueTask*>& whenMap)
{
    FFRT_COND_DO_ERR(isExit_, return 0, "cannot remove task, [queueId=%u] is exiting", queueId_);

    int mapSize = ClearWhenMap(whenMap, cond_);
    FFRT_LOGD("cancel [queueId=%u] all tasks succ, count %u", queueId_, mapSize);
    return mapSize;
}

int BaseQueue::Remove(const char* name)
{
    std::lock_guard lock(mutex_);
    return Remove(name, whenMap_);
}

int BaseQueue::Remove(const char* name, std::multimap<uint64_t, QueueTask*>& whenMap)
{
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot remove task, [queueId=%u] is exiting", queueId_);

    int removedCount = 0;
    for (auto iter = whenMap.begin(); iter != whenMap.end();) {
        if (iter->second->IsMatch(name)) {
            FFRT_LOGD("cancel task[%llu] %s succ", iter->second->gid, iter->second->GetLabel().c_str());
            iter->second->Cancel();
            iter = whenMap.erase(iter);
            removedCount++;
        } else {
            ++iter;
        }
    }

    return removedCount;
}

int BaseQueue::Remove(const QueueTask* task)
{
    std::lock_guard lock(mutex_);
    return Remove(task, whenMap_);
}

int BaseQueue::Remove(const QueueTask* task, std::multimap<uint64_t, QueueTask*>& whenMap)
{
    FFRT_COND_DO_ERR(isExit_, return FAILED, "cannot remove task, [queueId=%u] is exiting", queueId_);

    auto range = whenMap.equal_range(task->GetUptime());
    for (auto it = range.first; it != range.second; it++) {
        if (it->second == task) {
            whenMap.erase(it);
            return SUCC;
        }
    }

    return FAILED;
}

bool BaseQueue::HasTask(const char* name)
{
    std::lock_guard lock(mutex_);
    return HasTask(name, whenMap_);
}

bool BaseQueue::HasTask(const char* name, std::multimap<uint64_t, QueueTask*> whenMap)
{
    auto iter = std::find_if(whenMap.cbegin(), whenMap.cend(),
        [name](const auto& pair) { return pair.second->IsMatch(name); });
    return iter != whenMap.cend();
}

std::unique_ptr<BaseQueue> CreateQueue(int queueType, const ffrt_queue_attr_t* attr, const char* name)
{
    const auto iter = CREATE_FUNC_MAP.find(queueType);
    FFRT_COND_DO_ERR((iter == CREATE_FUNC_MAP.end()), return nullptr, "invalid queue type");

    return iter->second(attr, name);
}

uint64_t BaseQueue::GetDueTaskCount()
{
    std::lock_guard lock(mutex_);
    uint64_t count = GetDueTaskCount(whenMap_);
    if (count != 0) {
        FFRT_LOGD("qid = %llu Current Due Task Count %llu", GetQueueId(), count);
    }
    return count;
}

uint64_t BaseQueue::GetDueTaskCount(std::multimap<uint64_t, QueueTask*>& whenMap)
{
    const uint64_t& time = TimeStampSteady();
    auto it = whenMap.lower_bound(time);
    uint64_t count = static_cast<uint64_t>(std::distance(whenMap.begin(), it));
    return count;
}

std::vector<QueueTask*> BaseQueue::GetHeadTask()
{
    std::lock_guard lock(mutex_);
    if (whenMap_.empty()) {
        return {};
    }
    headTaskVec_[0] = whenMap_.begin()->second;
    return headTaskVec_;
}
} // namespace ffrt
