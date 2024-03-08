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
#include "queue_monitor.h"
#include <cstdint>
#include <sstream>
#include <iomanip>
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/osal.h"
#include "sync/sync.h"
#include "c/ffrt_watchdog.h"

namespace {
constexpr uint32_t INVALID_TASK_ID = 0;
constexpr uint32_t TIME_CONVERT_UNIT = 1000;
constexpr uint64_t QUEUE_INFO_INITIAL_CAPACITY = 64;
constexpr uint64_t ALLOW_TIME_ACC_ERROR_US = 500;
constexpr uint64_t MIN_TIMEOUT_THRESHOLD_US = 1000;

inline std::chrono::steady_clock::time_point GetDelayedTimeStamp(uint64_t delayUs)
{
    return std::chrono::steady_clock::now() + std::chrono::microseconds(delayUs);
}
}

namespace ffrt {
QueueMonitor::QueueMonitor()
{
    queuesRunningInfo_.reserve(QUEUE_INFO_INITIAL_CAPACITY);
    queuesStructInfo_.reserve(QUEUE_INFO_INITIAL_CAPACITY);
    uint64_t timeout = ffrt_watchdog_get_timeout() * TIME_CONVERT_UNIT;
    if (timeout < MIN_TIMEOUT_THRESHOLD_US) {
        timeoutUs_ = 0;
        FFRT_LOGE("failed to setup watchdog because [%llu] us less than precision threshold", timeout);
        return;
    }
    timeoutUs_ = timeout;
    SendDelayedWorker(GetDelayedTimeStamp(timeoutUs_));
    FFRT_LOGI("send delayedworker with %llu us", timeoutUs_);
}

QueueMonitor::~QueueMonitor()
{
    FFRT_LOGW("destruction of QueueMonitor enter");
    for (uint32_t id = 0; id < queuesRunningInfo_.size(); ++id) {
        if (queuesRunningInfo_[id].first != INVALID_TASK_ID) {
            usleep(MIN_TIMEOUT_THRESHOLD_US);
            break;
        }
    }
    FFRT_LOGW("destruction of QueueMonitor leave");
}

QueueMonitor& QueueMonitor::GetInstance()
{
    static QueueMonitor instance;
    return instance;
}

void QueueMonitor::RegisterQueueId(uint32_t queueId, SerialHandler* queueStruct)
{
    std::unique_lock lock(mutex_);
    if (queueId == queuesRunningInfo_.size()) {
        queuesRunningInfo_.emplace_back(std::make_pair(INVALID_TASK_ID, std::chrono::steady_clock::now()));
        queuesStructInfo_.emplace_back(queueStruct);
        FFRT_LOGD("queue registration in monitor gid=%u in turn succ", queueId);
        return;
    }

    // only need to ensure that the corresponding info index has been initialized after constructed.
    if (queueId > queuesRunningInfo_.size()) {
        for (uint32_t i = queuesRunningInfo_.size(); i <= queueId; ++i) {
            queuesRunningInfo_.emplace_back(std::make_pair(INVALID_TASK_ID, std::chrono::steady_clock::now()));
            queuesStructInfo_.emplace_back(nullptr);
        }
        queuesStructInfo_[queueId] = queueStruct;
    }
    if (queuesStructInfo_[queueId] == nullptr) {
        queuesStructInfo_[queueId] = queueStruct;
    }
    FFRT_LOGD("queue registration in monitor gid=%u by skip succ", queueId);
}

void QueueMonitor::ResetQueueInfo(uint32_t queueId)
{
    std::shared_lock lock(mutex_);
    FFRT_COND_DO_ERR((queuesRunningInfo_.size() <= queueId), return,
        "ResetQueueInfo queueId=%u access violation, RunningInfo_.size=%u", queueId, queuesRunningInfo_.size());
    queuesRunningInfo_[queueId].first = INVALID_TASK_ID;
}

void QueueMonitor::ResetQueueStruct(uint32_t queueId)
{
    std::shared_lock lock(mutex_);
    FFRT_COND_DO_ERR((queuesStructInfo_.size() <= queueId), return,
        "ResetQueueStruct queueId=%u access violation, StructInfo_.size=%u", queueId, queuesStructInfo_.size());
    queuesStructInfo_[queueId] = nullptr;
}

void QueueMonitor::UpdateQueueInfo(uint32_t queueId, const uint64_t &taskId)
{
    std::shared_lock lock(mutex_);
    FFRT_COND_DO_ERR((queuesRunningInfo_.size() <= queueId), return,
        "UpdateQueueInfo queueId=%u access violation, RunningInfo_.size=%u", queueId, queuesRunningInfo_.size());
    queuesRunningInfo_[queueId] = {taskId, std::chrono::steady_clock::now()};
}

uint64_t QueueMonitor::QueryQueueStatus(uint32_t queueId)
{
    std::shared_lock lock(mutex_);
    return queuesRunningInfo_[queueId].first;
}

void QueueMonitor::SendDelayedWorker(time_point_t delay)
{
    static WaitUntilEntry we;
    we.tp = delay;
    we.cb = ([this](WaitEntry* we) { CheckQueuesStatus(); });

    bool result = DelayedWakeup(we.tp, &we, we.cb);
    // insurance mechanism, generally does not fail
    while (!result) {
        FFRT_LOGW("failed to set delayedworker because the given timestamp has passed");
        we.tp = GetDelayedTimeStamp(ALLOW_TIME_ACC_ERROR_US);
        result = DelayedWakeup(we.tp, &we, we.cb);
    }
}

void QueueMonitor::ResetTaskTimestampAfterWarning(uint32_t queueId, const uint64_t &taskId)
{
    std::unique_lock lock(mutex_);
    if (queuesRunningInfo_[queueId].first == taskId) {
        queuesRunningInfo_[queueId].second += std::chrono::microseconds(timeoutUs_);
    }
}

void QueueMonitor::CheckQueuesStatus()
{
    time_point_t oldestStartedTime = std::chrono::steady_clock::now();
    time_point_t startThreshold = oldestStartedTime - std::chrono::microseconds(timeoutUs_ - ALLOW_TIME_ACC_ERROR_US);

    uint64_t taskId = 0;
    time_point_t taskTimestamp = oldestStartedTime;
    for (uint32_t i = 0; i < queuesRunningInfo_.size(); ++i) {
        {
            std::unique_lock lock(mutex_);
            taskId = queuesRunningInfo_[i].first;
            taskTimestamp = queuesRunningInfo_[i].second;
        }

        if (taskId == INVALID_TASK_ID) {
            continue;
        }

        if (taskTimestamp < startThreshold) {
            std::stringstream ss;
            ss << "SERIAL_TASK_TIMEOUT: serial queue qid=" << i << ", serial task gid=" << taskId << " execution " <<
                timeoutUs_ << " us.";
            if (queuesStructInfo_[i] != nullptr) {
                ss << queuesStructInfo_[i]->GetDfxInfo();
            }
            FFRT_LOGE("%s", ss.str().c_str());

            ffrt_watchdog_cb func = ffrt_watchdog_get_cb();
            if (func) {
                func(taskId, ss.str().c_str(), ss.str().size());
            }
            // reset timeout task timestamp for next warning
            ResetTaskTimestampAfterWarning(i, taskId);
            continue;
        }

        if (taskTimestamp < oldestStartedTime) {
            oldestStartedTime = taskTimestamp;
        }
    }

    time_point_t nextCheckTime = oldestStartedTime + std::chrono::microseconds(timeoutUs_);
    SendDelayedWorker(nextCheckTime);
    FFRT_LOGD("global watchdog completed queue status check and scheduled the next");
}
} // namespace ffrt
