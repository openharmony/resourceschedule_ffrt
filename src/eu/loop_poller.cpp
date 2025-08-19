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
#include "eu/loop_poller.h"
#include <securec.h>
#include "sched/execute_ctx.h"
#include "tm/scpu_task.h"
#include "dfx/log/ffrt_log_api.h"

constexpr uint64_t MAX_TIMER_MS_COUNT = 1000ULL * 100 * 60 * 60 * 24 * 365; // 100 year

namespace ffrt {
LoopPoller::~LoopPoller() noexcept
{
    timerHandle_ = -1;
    {
        std::lock_guard lg(mapMutex_);
        wakeDataMap_.clear();
        delCntMap_.clear();
        waitTaskMap_.clear();
        cachedTaskEvents_.clear();
    }
    {
        std::lock_guard lg(timerMutex_);
        timerMap_.clear();
        executedHandle_.clear();
    }
    flag_ = EpollStatus::TEARDOWN;
}

PollerProxy& PollerProxy::Instance()
{
    static PollerProxy pollerInstance;
    return pollerInstance;
}

void LoopPoller::ProcessWaitedFds(int nfds, std::unordered_map<CoTask*, EventVec>& syncTaskEvents,
                              std::array<epoll_event, EPOLL_EVENT_SIZE>& waitedEvents) noexcept
{
    for (unsigned int i = 0; i < static_cast<unsigned int>(nfds); ++i) {
        struct PollerData* data = reinterpret_cast<struct PollerData*>(waitedEvents[i].data.ptr);
        int currFd = data->fd;
        if (currFd == wakeData_.fd) {
            uint64_t one = 1;
            (void)::read(wakeData_.fd, &one, sizeof one);
            continue;
        }

        if (data->cb != nullptr) {
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (data->traceId.valid == HITRACE_ID_VALID) {
                TraceChainAdapter::Instance().HiTraceChainRestoreId(&data->traceId);
            }
#endif
            data->cb(data->data, waitedEvents[i].events);
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (data->traceId.valid == HITRACE_ID_VALID) {
                TraceChainAdapter::Instance().HiTraceChainClearId();
            }
#endif
            continue;
        }

        if (data->task != nullptr) {
            epoll_event ev = { .events = waitedEvents[i].events, .data = {.fd = currFd} };
            syncTaskEvents[data->task].push_back(ev);
            if (waitedEvents[i].events & (EPOLLHUP | EPOLLERR)) {
                std::lock_guard lg(mapMutex_);
                CacheMaskFdAndEpollDel(currFd, data->task);
            }
        }
    }
}

PollerRet LoopPoller::FindAndExecuteTimer(int timerHandle)
{
    if (timerHandle != -1) {
        timerMutex_.lock();
        for (auto it = timerMap_.begin(); it != timerMap_.end(); it++) {
            if (it->second.handle == timerHandle) {
                ExecuteTimerCb(it->first);
                return PollerRet::RET_TIMER;
            }
        }
        timerMutex_.unlock();
    }
    return PollerRet::RET_NULL;
}

PollerRet LoopPoller::PollOnce(int timeout) noexcept
{
    int realTimeout = timeout;
    int timerHandle = -1;

    timerMutex_.lock();
    if (!timerMap_.empty()) {
        auto cur = timerMap_.begin();
        timerHandle = cur->second.handle;
        TimePoint now = std::chrono::steady_clock::now();
        realTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(
            cur->first - now).count();
        if (realTimeout <= 0) {
            ExecuteTimerCb(now);
            return PollerRet::RET_TIMER;
        }

        if (timeout != -1 && realTimeout > timeout) {
            timerHandle = -1;
            realTimeout = timeout;
        }

        flag_ = EpollStatus::WAIT;
    }
    timerMutex_.unlock();

    pollerCount_++;
    std::array<epoll_event, EPOLL_EVENT_SIZE> waitedEvents;
    int nfds = epoll_wait(epFd_, waitedEvents.data(), waitedEvents.size(), realTimeout);
    flag_ = EpollStatus::WAKE;
    if (nfds < 0) {
        if (errno != EINTR) {
            FFRT_SYSEVENT_LOGE("epoll_wait error, errorno= %d.", errno);
        }
        return PollerRet::RET_NULL;
    }
    if (nfds == 0) {
        return FindAndExecuteTimer(timerHandle);
    }

    std::unordered_map<CoTask*, EventVec> syncTaskEvents;
    ProcessWaitedFds(nfds, syncTaskEvents, waitedEvents);
    WakeSyncTask(syncTaskEvents);
    ReleaseFdWakeData();
    return PollerRet::RET_EPOLL;
}

void LoopPoller::ProcessTimerDataCb(CoTask* task) noexcept
{
    mapMutex_.lock();
    auto iter = waitTaskMap_.find(task);
    if (iter != waitTaskMap_.end()) {
        WakeTask(task);
        waitTaskMap_.erase(iter);
    }
    mapMutex_.unlock();
}

void LoopPoller::ExecuteTimerCb(TimePoint timer) noexcept
{
    while (!timerMap_.empty()) {
        auto iter = timerMap_.begin();
        if (iter->first > timer) {
            break;
        }

        TimerDataWithCb data = iter->second;
        if (data.cb != nullptr) {
            executedHandle_[data.handle] = TimerStatus::EXECUTING;
        }

        timerMap_.erase(iter);
        timerEmpty_.store(timerMap_.empty());

        if (data.cb != nullptr) {
            timerMutex_.unlock();
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (data.traceId.valid == HITRACE_ID_VALID) {
                TraceChainAdapter::Instance().HiTraceChainRestoreId(&data.traceId);
            }
#endif
            data.cb(data.data);
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (data.traceId.valid == HITRACE_ID_VALID) {
                TraceChainAdapter::Instance().HiTraceChainClearId();
            }
#endif
            timerMutex_.lock();
            executedHandle_[data.handle] = TimerStatus::EXECUTED;
        } else if (data.task != nullptr) {
            timerMutex_.unlock();
            ProcessTimerDataCb(data.task);
            timerMutex_.lock();
        }

        if (data.repeat && (executedHandle_.find(data.handle) != executedHandle_.end())) {
            executedHandle_.erase(data.handle);
            RegisterTimerImpl(data);
        }
    }
    timerMutex_.unlock();
}

void LoopPoller::RegisterTimerImpl(const TimerDataWithCb& data) noexcept
{
    if (flag_ == EpollStatus::TEARDOWN) {
        return;
    }

    TimePoint absoluteTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(data.timeout);
    bool wake = timerMap_.empty() || (absoluteTime < timerMap_.begin()->first && flag_ == EpollStatus::WAIT);

    timerMap_.emplace(absoluteTime, data);
    timerEmpty_.store(false);

    if (wake) {
        WakeUp();
    }
}

int LoopPoller::RegisterTimer(uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat) noexcept
{
    if (flag_ == EpollStatus::TEARDOWN) {
        return -1;
    }

    if (timeout > MAX_TIMER_MS_COUNT) {
        FFRT_LOGW("timeout exceeds maximum allowed value %llu ms. Clamping to %llu ms.", timeout, MAX_TIMER_MS_COUNT);
        timeout = MAX_TIMER_MS_COUNT;
    }

    std::lock_guard lock(timerMutex_);
    timerHandle_ += 1;

    CoTask* task = IsCoTask(ExecuteCtx::Cur()->task) ? static_cast<CoTask*>(ExecuteCtx::Cur()->task) : nullptr; 
    TimerDataWithCb timerMapValue(data, cb, task, repeat, timeout);
    timerMapValue.handle = timerHandle_;
    RegisterTimerImpl(timerMapValue);

    return timerHandle_;
}

int LoopPoller::UnregisterTimer(int handle) noexcept
{
    if (flag_ == EpollStatus::TEARDOWN) {
        return -1;
    }

    std::lock_guard lock(timerMutex_);
    auto it = executedHandle_.find(handle);
    if (it != executedHandle_.end()) {
        while (it->second == TimerStatus::EXECUTING) {
            timerMutex_.unlock();
            std::this_thread::yield();
            timerMutex_.lock();
            it = executedHandle_.find(handle);
            if (it == executedHandle_.end()) {
                break;
            }
        }

        if (it != executedHandle_.end()) {
            executedHandle_.erase(it);
            return 0;
        }
    }

    bool wake = false;
    int ret = -1;
    for (auto cur = timerMap_.begin(); cur != timerMap_.end(); cur++) {
        if (cur->second.handle == handle) {
            if (cur == timerMap_.begin() && flag_ == EpollStatus::WAIT) {
                wake = true;
            }
            timerMap_.erase(cur);
            ret = 0;
            break;
        }
    }

    timerEmpty_.store(timerMap_.empty());

    if (wake) {
        WakeUp();
    }
    return ret;
}

bool LoopPoller::DetermineEmptyMap() noexcept
{
    return fdEmpty_ && timerEmpty_;
}

bool LoopPoller::DeterminePollerReady() noexcept
{
    return IsFdExist() || IsTimerReady();
}

bool LoopPoller::IsFdExist() noexcept
{
    return !fdEmpty_;
}

bool LoopPoller::IsTimerReady() noexcept
{
    TimePoint now = std::chrono::steady_clock::now();
    std::lock_guard lock(timerMutex_);
    if (timerMap_.empty()) {
        return false;
    }

    if (now >= timerMap_.begin()->first) {
        return true;
    }
    return false;
}

ffrt_timer_query_t LoopPoller::GetTimerStatus(int handle) noexcept
{
    if (flag_ == EpollStatus::TEARDOWN) {
        return ffrt_timer_notfound;
    }

    std::lock_guard lock(timerMutex_);
    for (auto cur = timerMap_.begin(); cur != timerMap_.end(); cur++) {
        if (cur->second.handle == handle) {
            return ffrt_timer_not_executed;
        }
    }

    auto it = executedHandle_.find(handle);
    if (it != executedHandle_.end()) {
        while (it->second == TimerStatus::EXECUTING) {
            timerMutex_.unlock();
            std::this_thread::yield();
            timerMutex_.lock();
            it = executedHandle_.find(handle);
            if (it == executedHandle_.end()) {
                break;
            }
        }
        return ffrt_timer_executed;
    }

    return ffrt_timer_notfound;
}
}