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
#include "queue_handler.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/slab.h"
#include "sync/sync.h"
#include "c/ffrt_dump.h"
#include "c/queue.h"
#include "internal_inc/osal.h"
#include "util/ffrt_facade.h"
#include "util/time_format.h"

namespace {
constexpr uint32_t US_PER_MS = 1000;
constexpr uint64_t ALLOW_ACC_ERROR_US = 10 * US_PER_MS; // 10ms
constexpr uint64_t MIN_TIMEOUT_THRESHOLD_US = 1000 * US_PER_MS; // 1s
}

namespace ffrt {
QueueMonitor::QueueMonitor()
{
    FFRT_LOGI("QueueMonitor ctor enter");
    we_ = new (SimpleAllocator<WaitUntilEntry>::AllocMem()) WaitUntilEntry();
    uint64_t timeout = ffrt_task_timeout_get_threshold() * US_PER_MS;
    timeoutUs_ = timeout;
    if (timeout < MIN_TIMEOUT_THRESHOLD_US) {
        FFRT_LOGE("invalid watchdog timeout [%llu] us, using 1s instead", timeout);
        timeoutUs_ = MIN_TIMEOUT_THRESHOLD_US;
    }
    FFRT_LOGI("QueueMonitor ctor leave, watchdog timeout of %llu us has been set", timeoutUs_);
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

void QueueMonitor::ResetQueue(QueueHandler* queue)
{
    std::unique_lock lock(infoMutex_);
    queuesInfo_.push_back(queue);
    FFRT_LOGD("queue [%s] register in QueueMonitor", queue->GetName().c_str());
}

void QueueMonitor::DeregisterQueue(QueueHandler* queue)
{
    std::unique_lock lock(infoMutex_);
    auto it = std::find(queuesInfo_.begin(), queuesInfo_.end(), queue);
    if (it != queuesInfo_.end()) {
        queuesInfo_.erase(it);
    }
}

uint64_t QueueMonitor::UpdateQueueInfo()
{
    std::shared_lock lock(infoMutex_);
    if (suspendAlarm_.exchange(false)) {
        uint64_t alarmTime = static_cast<uint64_t>(std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now()).time_since_epoch().count()) + timeoutUs_;
        SetAlarm(alarmTime);
    }
}

void QueueMonitor::SetAlarm(uint64_t steadyUs)
{
    we_->tp = std::chrono::steady_clock::time_point() + std::chrono::microseconds(steadyUs);
    we_->cb = ([this](WaitEntry* we_) { ScheduleAlarm(); });

    bool result = DelayedWakeup(we_->tp, we_, we_->cb);
    // generally does not fail
    while (!result) {
        FFRT_LOGW("failed to set delayedworker, try again");
        we_->tp = std::chrono::steady_clock::now() + std::chrono::microseconds(ALLOW_ACC_ERROR_US);
        result = DelayedWakeup(we_->tp, we_, we_->cb);
    }
}

void QueueMonitor::ScheduleAlarm()
{
    uint64_t nextTaskStart = UINT64_MAX;
    CheckTimeout(nextTaskStart);
    FFRT_LOGD("queue monitor checked, going next");
    // 所有队列都没有任务，暂停定时器
    if (nextTaskStart == UINT64_MAX) {
        suspendAlarm_.exchange(true);
        return;
    }

    SetAlarm(nextTaskStart + timeoutUs_);
}

void QueueMonitor::CheckTimeout(uint64_t& nextTaskStart)
{
    std::unique_lock lock(infoMutex_);

    // 未来ALLOW_ACC_ERROR_US可能超时的任务，一起上报
    uint64_t now = TimeStampCntvct();
    uint64_t minStart = now - ((timeoutUs_ - ALLOW_ACC_ERROR_US));
    for (auto& queueInfo : queuesInfo_) {
        // first为gid, second为下次触发超时的时间
        std::pair<uint64_t, uint64_t> curTaskTimeStamp = queueInfo->EvaluateTaskTimeout(minStart, timeoutUs_,
            timeoutMSG_);
        if (curTaskTimeStamp.second < UINT64_MAX && curTaskTimeStamp.first != 0) {
            ReportEventTimeout(curTaskTimeStamp.first, timeoutMSG_);
        }

        if (curTaskTimeStamp.second < nextTaskStart) {
            nextTaskStart = curTaskTimeStamp.second;
        }
    }
}

void QueueMonitor::ReportEventTimeout(uint64_t curGid, const std::stringstream& ss)
{
    std::string ssStr = ss.str();
    if (ffrt_task_timeout_get_cb()) {
        FFRTFacade::GetDWInstance().SubmitAsyncTask([curGid, ssStr] {
            ffrt_task_timeout_cb func = ffrt_task_timeout_get_cb();
            if (func) {
                func(curGid, ssStr.c_str(), ssStr.size());
            }
        });
    }
}

} // namespace ffrt
