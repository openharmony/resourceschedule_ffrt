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
#ifndef FFRT_QUEUE_MONITOR_H
#define FFRT_QUEUE_MONITOR_H

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>
#include "sched/execute_ctx.h"

namespace ffrt {
constexpr uint32_t INVAILD_TASK_ID = 0;
class QueueMonitor {
public:
    static QueueMonitor &GetInstance();
    void RegisterQueueId(const uint32_t &queueId);

    inline void ResetQueueInfo(const uint32_t &queueId)
    {
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
        std::shared_lock lock(mutex_);
        QueuesRunningInfo[queueId].first = INVAILD_TASK_ID;
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
    }

    inline void UpdateQueueInfo(const uint32_t &queueId, const uint64_t &taskId)
    {
#ifdef FRRT_CO_BACKTRACE_OH_ENABLE
        std::shared_lock lock(mutex_);
        QueuesRunningInfo[queueId] = {taskId, std::chrono::steady_clock::now()};
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
    }

private:
    QueueMonitor();
    ~QueueMonitor() = default;
    QueueMonitor(const QueueMonitor &) = delete;
    QueueMonitor(QueueMonitor &&) = delete;
    QueueMonitor &operator=(const QueueMonitor &) = delete;
    QueueMonitor &operator=(QueueMonitor &&) = delete;

    void SendDelayedWorker(time_point_t delay);
    void CheckQueuesStatus();

    uint64_t timeoutUs_ = 0;
    std::shared_mutex mutex_;
    std::vector<std::pair<uint64_t, time_point_t>> QueuesRunningInfo;
};
} // namespace ffrt

#endif // FFRT_QUEUE_MONITOR_H