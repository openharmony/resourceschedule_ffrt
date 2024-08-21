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

namespace {
constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
constexpr uint32_t INVALID_TASK_ID = 0;
constexpr uint32_t TIME_CONVERT_UNIT = 1000;
constexpr uint64_t QUEUE_INFO_INITIAL_CAPACITY = 64;
constexpr uint64_t ALLOW_TIME_ACC_ERROR_US = 500;
constexpr uint64_t MIN_TIMEOUT_THRESHOLD_US = 1000;
constexpr uint64_t DESTRUCT_TRY_COUNT = 100;

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
    uint64_t timeout = ffrt_task_timeout_get_threshold() * TIME_CONVERT_UNIT;
    if (timeout < MIN_TIMEOUT_THRESHOLD_US) {
        timeoutUs_ = 0;
        FFRT_LOGE("failed to setup watchdog because [%llu] us less than precision threshold", timeout);
        return;
    }
    timeoutUs_ = timeout;
    SendDelayedWorker(GetDelayedTimeStamp(timeoutUs_));
    FFRT_LOGI("queue monitor ctor leave, watchdog timeout %llu us", timeoutUs_);
}

QueueMonitor::~QueueMonitor()
{
    exit_.store(true);
    FFRT_LOGI("destruction of QueueMonitor enter");
    int tryCnt = DESTRUCT_TRY_COUNT;
    // 取消定时器成功，或者中断了发送定时器，则释放we完成析构
    while (!DelayedRemove(we_->tp, we_) && !abortSendTimer_.load()) {
        if (--tryCnt < 0) {
            break;
        }
        usleep(MIN_TIMEOUT_THRESHOLD_US);
    }
    SimpleAllocator<WaitUntilEntry>::FreeMem(we_);
    FFRT_LOGI("destruction of QueueMonitor leave");
}

QueueMonitor& QueueMonitor::GetInstance()
{
    static QueueMonitor instance;
    return instance;
}

void QueueMonitor::RegisterQueueId(uint32_t queueId, QueueHandler* queueStruct)
{
    std::unique_lock lock(mutex_);
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
    std::shared_lock lock(mutex_);
    FFRT_COND_DO_ERR((queuesRunningInfo_.size() <= queueId), return,
        "ResetQueueInfo queueId=%u access violation, RunningInfo_.size=%u", queueId, queuesRunningInfo_.size());
    queuesRunningInfo_[queueId].first = INVALID_TASK_ID;
    lastReportedTask_[queueId] = INVALID_TASK_ID;
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
    TimePoint now = std::chrono::steady_clock::now();
    queuesRunningInfo_[queueId] = {taskId, now};
    if (exit_.exchange(false)) {
        SendDelayedWorker(now + std::chrono::microseconds(timeoutUs_));
    }
}

uint64_t QueueMonitor::QueryQueueStatus(uint32_t queueId)
{
    std::shared_lock lock(mutex_);
    FFRT_COND_DO_ERR((queuesRunningInfo_.size() <= queueId), return INVALID_TASK_ID,
        "QueryQueueStatus queueId=%u access violation, RunningInfo_.size=%u", queueId, queuesRunningInfo_.size());
    return queuesRunningInfo_[queueId].first;
}

void QueueMonitor::SendDelayedWorker(TimePoint delay)
{
    FFRT_COND_DO_ERR(exit_.load(), abortSendTimer_.store(true);
        return;,
        "exit_.load() is true");

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
    std::unique_lock lock(mutex_);
    if (queuesRunningInfo_[queueId].first == taskId) {
        queuesRunningInfo_[queueId].second += std::chrono::microseconds(timeoutUs_);
    }
}

void QueueMonitor::CheckQueuesStatus()
{
    {
        std::unique_lock lock(mutex_);
        auto iter = std::find_if(queuesRunningInfo_.cbegin(), queuesRunningInfo_.cend(),
            [](const auto& pair) { return pair.first != INVALID_TASK_ID; });
        if (iter == queuesRunningInfo_.cend()) {
            exit_ = true;
            return;
        }
    }

    TimePoint oldestStartedTime = std::chrono::steady_clock::now();
    TimePoint startThreshold = oldestStartedTime - std::chrono::microseconds(timeoutUs_ - ALLOW_TIME_ACC_ERROR_US);
    uint64_t taskId = 0;
    uint32_t queueRunningInfoSize = 0;
    TimePoint taskTimestamp = oldestStartedTime;
    {
        std::shared_lock lock(mutex_);
        queueRunningInfoSize = queuesRunningInfo_.size();
    }
    for (uint32_t i = 0; i < queueRunningInfoSize; ++i) {
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
            char processName[PROCESS_NAME_BUFFER_LENGTH];
            GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
            ss << "Serial_Queue_Timeout, process name:[" << processName << "], serial queue qid:[" << i
                << "], serial task gid:[" << taskId << "], execution:[" << timeoutUs_ << "] us.";
            if (queuesStructInfo_[i] != nullptr) {
                ss << queuesStructInfo_[i]->GetDfxInfo();
            }
            FFRT_LOGE("%s", ss.str().c_str());
#ifdef FFRT_SEND_EVENT
            if (lastReportedTask_[i] != taskId) {
                lastReportedTask_[i] = taskId;
                std::string processNameStr = std::string(processName);
                std::string senarioName = "Serial_Queue_Timeout";
                TaskTimeoutReport(ss, processNameStr, senarioName);
            }
#endif          
            ffrt_task_timeout_cb func = ffrt_task_timeout_get_cb();
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

    SendDelayedWorker(oldestStartedTime + std::chrono::microseconds(timeoutUs_));
    FFRT_LOGD("global watchdog completed queue status check and scheduled the next");
}

bool QueueMonitor::HasQueueActive()
{
    std::unique_lock lock(mutex_);
    for (uint32_t i = 0; i < queuesRunningInfo_.size(); ++i) {
        if (queuesRunningInfo_[i].first != INVALID_TASK_ID) {
            return true;
        }
    }
    return false;
}
} // namespace ffrt
