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
#include <sstream>
#include "dfx/log/ffrt_log_api.h"
#include "util/slab.h"
#include "sync/sync.h"
#include "c/ffrt_dump.h"
#include "dfx/sysevent/sysevent.h"
#include "internal_inc/osal.h"
#include "util/ffrt_facade.h"

namespace {
constexpr uint32_t INVALID_TASK_ID = 0;
constexpr uint32_t TIME_CONVERT_UNIT = 1000;
constexpr uint64_t QUEUE_INFO_INITIAL_CAPACITY = 64;
constexpr uint64_t ALLOW_TIME_ACC_ERROR_US = 500;

inline std::chrono::steady_clock::time_point GetDelayedTimeStamp(uint64_t delayUs)
{
    return std::chrono::steady_clock::now() + std::chrono::microseconds(delayUs);
}
}

namespace ffrt {
QueueMonitor::QueueMonitor()
{
    FFRT_LOGI("queue monitor ctor enter");
    queuesRunningInfo_.reserve(QUEUE_INFO_INITIAL_CAPACITY);
    queuesStructInfo_.reserve(QUEUE_INFO_INITIAL_CAPACITY);
    lastReportedTask_.reserve(QUEUE_INFO_INITIAL_CAPACITY);
    we_ = new (SimpleAllocator<WaitUntilEntry>::AllocMem()) WaitUntilEntry();
    timeoutUs_ = ffrt_task_timeout_get_threshold() * TIME_CONVERT_UNIT;
    FFRT_LOGI("queue monitor ctor leave, watchdog timeout %llu us", timeoutUs_);
}

QueueMonitor::~QueueMonitor()
{
    FFRT_LOGI("destruction of QueueMonitor");
    DelayedRemove(we_->tp, we_);
    SimpleAllocator<WaitUntilEntry>::FreeMem(we_);
}

QueueMonitor& QueueMonitor::GetInstance()
{
    static QueueMonitor instance;
    return instance;
}

void QueueMonitor::RegisterQueueId(uint32_t queueId, QueueHandler* queueStruct)
{
    // acquire shared mutex in exclusive mode, because queuesRunningInfo_
    // will be modified
    std::lock_guard lock(mutex_);
    if (queueId == queuesRunningInfo_.size()) {
        queuesRunningInfo_.emplace_back(std::make_pair(INVALID_TASK_ID, std::chrono::steady_clock::now()));
        queuesStructInfo_.emplace_back(queueStruct);
        lastReportedTask_.emplace_back(INVALID_TASK_ID);
        FFRT_LOGD("queue registration in monitor gid=%u in turn succ", queueId);
        return;
    }

    // only need to ensure that the corresponding info index has been initialized after constructed.
    if (queueId > queuesRunningInfo_.size()) {
        for (uint32_t i = queuesRunningInfo_.size(); i <= queueId; ++i) {
            queuesRunningInfo_.emplace_back(std::make_pair(INVALID_TASK_ID, std::chrono::steady_clock::now()));
            queuesStructInfo_.emplace_back(nullptr);
            lastReportedTask_.emplace_back(INVALID_TASK_ID);
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
    // acquire shared mutex in exclusive mode, because queuesRunningInfo_
    // will be modified
    std::lock_guard lock(mutex_);
    FFRT_COND_DO_ERR((queuesRunningInfo_.size() <= queueId), return,
        "ResetQueueInfo queueId=%u access violation, RunningInfo_.size=%u", queueId, queuesRunningInfo_.size());
    queuesRunningInfo_[queueId].first = INVALID_TASK_ID;
    lastReportedTask_[queueId] = INVALID_TASK_ID;
}

void QueueMonitor::ResetQueueStruct(uint32_t queueId)
{
    // acquire shared mutex in exclusive mode, because queuesRunningInfo_
    // will be modified
    std::lock_guard lock(mutex_);
    FFRT_COND_DO_ERR((queuesStructInfo_.size() <= queueId), return,
        "ResetQueueStruct queueId=%u access violation, StructInfo_.size=%u", queueId, queuesStructInfo_.size());
    queuesStructInfo_[queueId] = nullptr;
}

void QueueMonitor::UpdateQueueInfo(uint32_t queueId, const uint64_t &taskId)
{
    // acquire shared mutex in exclusive mode, because queuesRunningInfo_
    // will be modified
    std::lock_guard lock(mutex_);
    FFRT_COND_DO_ERR((queuesRunningInfo_.size() <= queueId), return,
        "UpdateQueueInfo queueId=%u access violation, RunningInfo_.size=%u", queueId, queuesRunningInfo_.size());
    TimePoint now = std::chrono::steady_clock::now();
    queuesRunningInfo_[queueId] = {taskId, now};
    if (exit_.exchange(false)) {
        UpdateTimeoutUs();
        SendDelayedWorker(now + std::chrono::microseconds(timeoutUs_));
    }
}

uint64_t QueueMonitor::QueryQueueStatus(uint32_t queueId)
{
    // acquire shared mutex in shared mode, because we only read
    std::shared_lock lock(mutex_);
    FFRT_COND_DO_ERR((queuesRunningInfo_.size() <= queueId), return INVALID_TASK_ID,
        "QueryQueueStatus queueId=%u access violation, RunningInfo_.size=%u", queueId, queuesRunningInfo_.size());
    return queuesRunningInfo_[queueId].first;
}

void QueueMonitor::SendDelayedWorker(TimePoint delay)
{
    we_->tp = delay;
    we_->cb = ([this](WaitEntry* we_) { CheckQueuesStatus(); });

    bool result = DelayedWakeup(we_->tp, we_, we_->cb);
    // insurance mechanism, generally does not fail
    while (!result) {
        FFRT_LOGW("failed to set delayedworker because the given timestamp has passed");
        we_->tp = GetDelayedTimeStamp(ALLOW_TIME_ACC_ERROR_US);
        result = DelayedWakeup(we_->tp, we_, we_->cb);
    }
}

void QueueMonitor::ResetTaskTimestampAfterWarning(uint32_t queueId, const uint64_t &taskId)
{
    // acquire shared mutex in exclusive mode, because queuesRunningInfo_
    // may be modified
    std::lock_guard lock(mutex_);
    if (queuesRunningInfo_[queueId].first == taskId) {
        queuesRunningInfo_[queueId].second += std::chrono::microseconds(timeoutUs_);
    }
}

void QueueMonitor::CheckQueuesStatus()
{
    {
        // acquire shared mutex in shared mode, because we only read
        std::shared_lock lock(mutex_);
        auto iter = std::find_if(queuesRunningInfo_.cbegin(), queuesRunningInfo_.cend(),
            [](const auto& pair) { return pair.first != INVALID_TASK_ID; });
        if (iter == queuesRunningInfo_.cend()) {
            exit_ = true;
            return;
        }
    }

    UpdateTimeoutUs();
    TimePoint oldestStartedTime = std::chrono::steady_clock::now();
    TimePoint startThreshold = oldestStartedTime - std::chrono::microseconds(timeoutUs_ - ALLOW_TIME_ACC_ERROR_US);
    uint64_t taskId = 0;
    uint32_t queueRunningInfoSize = 0;
    TimePoint taskTimestamp = oldestStartedTime;
    {
        std::shared_lock lock(mutex_);
        queueRunningInfoSize = queuesRunningInfo_.size();
    }

    // Displays information about queues whose tasks time out.
    for (uint32_t i = 0; i < queueRunningInfoSize; ++i) {
        {
            // acquire shared mutex in shared mode, because we only read
            std::shared_lock lock(mutex_);
            taskId = queuesRunningInfo_[i].first;
            taskTimestamp = queuesRunningInfo_[i].second;
        }

        if (taskId == INVALID_TASK_ID) {
            continue;
        }

        if (taskTimestamp < startThreshold) {
            std::stringstream ss;
            std::string processNameStr = std::string(GetCurrentProcessName());
            ss << "Serial_Queue_Timeout, process name:[" << processNameStr << "], serial queue qid:[" << i
                << "], serial task gid:[" << taskId << "], execution:[" << timeoutUs_ << "] us.";
            {
                // acquire shared mutex in shared mode, because we only read
                std::shared_lock lock(mutex_);
                if (queuesStructInfo_[i] != nullptr) {
                    ss << queuesStructInfo_[i]->GetDfxInfo();
                }
            }
            FFRT_LOGE("%s", ss.str().c_str());
#ifdef FFRT_SEND_EVENT
            if (lastReportedTask_[i] != taskId) {
                lastReportedTask_[i] = taskId;

                std::string senarioName = "Serial_Queue_Timeout";
                TaskTimeoutReport(ss, processNameStr, senarioName);
            }
#endif
            std::string ssStr = ss.str();
            if (ffrt_task_timeout_get_cb()) {
                FFRTFacade::GetDWInstance().SubmitAsyncTask([taskId, ssStr] {
                    ffrt_task_timeout_cb func = ffrt_task_timeout_get_cb();
                    if (func) {
                        func(taskId, ssStr.c_str(), ssStr.size());
                    }
                });
            }
            // reset timeout task timestamp for next warning
            ResetTaskTimestampAfterWarning(i, taskId);
            continue;
        }

        if (taskTimestamp < oldestStartedTime) {
            oldestStartedTime = taskTimestamp;
        }
    }

    SendDelayedWorker(oldestStartedTime + std::chrono::microseconds(timeoutUs_));
    FFRT_LOGD("global watchdog completed queue status check and scheduled the next");
}

bool QueueMonitor::HasQueueActive()
{
    // acquire shared mutex in shared mode, because we only read
    std::shared_lock lock(mutex_);
    for (uint32_t i = 0; i < queuesRunningInfo_.size(); ++i) {
        if (queuesRunningInfo_[i].first != INVALID_TASK_ID) {
            return true;
        }
    }
    return false;
}

void QueueMonitor::UpdateTimeoutUs()
{
    timeoutUs_ = ffrt_task_timeout_get_threshold() * TIME_CONVERT_UNIT;
}
} // namespace ffrt
