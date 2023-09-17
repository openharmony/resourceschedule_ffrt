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
constexpr uint32_t INVAILD_TASK_ID = 0;
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
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    QueuesRunningInfo.reserve(QUEUE_INFO_INITIAL_CAPACITY);
    uint64_t timeout = ffrt_watchdog_get_timeout() * TIME_CONVERT_UNIT;
    if (timeout < MIN_TIMEOUT_THRESHOLD_US) {
        timeoutUs_ = 0;
        FFRT_LOGE("failed to setup watchdog because [%llu] us less than precision threshold", timeout);
        return;
    }
    timeoutUs_ = timeout;
    SendDelayedWorker(GetDelayedTimeStamp(timeoutUs_));
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

QueueMonitor& QueueMonitor::GetInstance()
{
    static QueueMonitor instance;
    return instance;
}

void QueueMonitor::RegisterQueueId(uint32_t queueId)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    if (queueId == QueuesRunningInfo.size()) {
        QueuesRunningInfo.emplace_back(std::make_pair(INVAILD_TASK_ID, std::chrono::steady_clock::now()));
        FFRT_LOGD("queue registration in monitor gid=%u in turn succ", queueId);
        return;
    }

    // only need to ensure that the corresponding info index has been initialized after constructed.
    if (queueId > QueuesRunningInfo.size()) {
        for (uint32_t i = QueuesRunningInfo.size(); i <= queueId; ++i) {
            QueuesRunningInfo.emplace_back(std::make_pair(INVAILD_TASK_ID, std::chrono::steady_clock::now()));
        }
    }
    FFRT_LOGD("queue registration in monitor gid=%u by skip succ", queueId);
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

void QueueMonitor::ResetQueueInfo(uint32_t queueId)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    std::shared_lock lock(mutex_);
    QueuesRunningInfo[queueId].first = INVAILD_TASK_ID;
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

void QueueMonitor::UpdateQueueInfo(uint32_t queueId, const uint64_t &taskId)
{
#ifdef FRRT_CO_BACKTRACE_OH_ENABLE
    std::shared_lock lock(mutex_);
    QueuesRunningInfo[queueId] = {taskId, std::chrono::steady_clock::now()};
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

void QueueMonitor::SendDelayedWorker(time_point_t delay)
{
#ifdef FRRT_CO_BACKTRACE_OH_ENABLE
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
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

void QueueMonitor::ResetTaskTimestampAfterWarning(uint32_t queueId, const uint64_t &taskId)
{
#ifdef FRRT_CO_BACKTRACE_OH_ENABLE
    std::unique_lock lock(mutex_);
    if (QueuesRunningInfo[i].first == taskId) {
        QueuesRunningInfo[i].second += std::chrono::microseconds(timeoutUs_);
    }
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

void QueueMonitor::CheckQueuesStatus()
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    time_point_t oldestStartedTime = std::chrono::steady_clock::now();
    time_point_t startThreshold = oldestStartedTime - std::chrono::microseconds(timeoutUs_ - ALLOW_TIME_ACC_ERROR_US);
    
    uint64_t taskId = 0;
    time_point_t taskTimestamp = oldestStartedTime;
    for (uint32_t i = 0; i < QueuesRunningInfo.size(); ++i) {
        {
            std::unique_lock lock(mutex_);
            taskId = QueuesRunningInfo[i].first;
            time_point_t taskTimestamp = QueuesRunningInfo[i].second;
        }
        
        if (taskId == INVAILD_TASK_ID) {
            continue;
        }
        
        if (taskTimestamp < startThreshold) {
            std::stringstream ss;
            ss << "SERIAL_TASK_TIMEOUT: serial queue qid=" << i <<
             ", serial task gid=" << taskId << " execution " << timeoutUs_ << "us.";
            FFRT_LOGE("%s", ss.str().c_str());

            ffrt_watchdog_cb func = ffrt_watchdog_get_cb();
            if (func) {
                func(taskId, ss.str().c_str(), ss.str().size());
            }
            // reset timeout task timestampe for next warning
            ResetTaskTimestampAfterWarning(i, taskId);
            continue;
        }
        
        if (taskTimestamp < oldestStartedTime) {
            oldestStartedTime = taskTimestamp;
        }
    }
    
    time_point_t nextCheckTime = oldestStartedTime + std::chrono::microseconds(timeoutUs_);
    SendDelayedWorker(nextCheckTime);
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}
} // namespace ffrt
