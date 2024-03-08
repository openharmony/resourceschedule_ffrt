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

#include <shared_mutex>
#include <string>
#include <vector>
#include "sched/execute_ctx.h"
#include "serial_handler.h"

namespace ffrt {
class QueueMonitor {
public:
    static QueueMonitor &GetInstance();
    void RegisterQueueId(uint32_t queueId, SerialHandler* queueStruct);
    void ResetQueueInfo(uint32_t queueId);
    void ResetQueueStruct(uint32_t queueId);
    void UpdateQueueInfo(uint32_t queueId, const uint64_t &taskId);
    uint64_t QueryQueueStatus(uint32_t queueId);

private:
    QueueMonitor();
    ~QueueMonitor();
    QueueMonitor(const QueueMonitor &) = delete;
    QueueMonitor(QueueMonitor &&) = delete;
    QueueMonitor &operator=(const QueueMonitor &) = delete;
    QueueMonitor &operator=(QueueMonitor &&) = delete;

    void SendDelayedWorker(time_point_t delay);
    void CheckQueuesStatus();
    void ResetTaskTimestampAfterWarning(uint32_t queueId, const uint64_t &taskId);

    uint64_t timeoutUs_ = 0;
    std::shared_mutex mutex_;
    std::vector<std::pair<uint64_t, time_point_t>> queuesRunningInfo_;
    std::vector<SerialHandler*> queuesStructInfo_;
};
} // namespace ffrt

#endif // FFRT_QUEUE_MONITOR_H