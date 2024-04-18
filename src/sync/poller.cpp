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
#include "poller.h"
#include "sched/execute_ctx.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
Poller::Poller() noexcept: m_epFd { ::epoll_create1(EPOLL_CLOEXEC) }
{
    m_wakeData.cb = nullptr;
    m_wakeData.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    epoll_event ev { .events = EPOLLIN, .data = { .ptr = static_cast<void*>(&m_wakeData) } };
    if (epoll_ctl(m_epFd, EPOLL_CTL_ADD, m_wakeData.fd, &ev) < 0) {
        std::terminate();
    }
}

Poller::~Poller() noexcept
{
    ::close(m_wakeData.fd);
    ::close(m_epFd);
    timerHandle_ = -1;
    m_wakeDataMap.clear();
    m_delCntMap.clear();
    timerMap_.clear();
    executedHandle_.clear();
    flag_ = EpollStatus::TEARDOWN;
}

PollerProxy* PollerProxy::Instance()
{
    static PollerProxy pollerInstance;
    return &pollerInstance;
}

int Poller::AddFdEvent(uint32_t events, int fd, void* data, ffrt_poller_cb cb) noexcept
{
    auto wakeData = std::unique_ptr<WakeDataWithCb>(new (std::nothrow) WakeDataWithCb(fd, data, cb));
    void* ptr = static_cast<void*>(wakeData.get());
    if (ptr == nullptr || wakeData == nullptr) {
        FFRT_LOGE("Construct WakeDataWithCb instance failed! or wakeData is nullptr");
        return -1;
    }

    epoll_event ev = { .events = events, .data = { .ptr = ptr } };
    if (epoll_ctl(m_epFd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        FFRT_LOGE("epoll_ctl add fd error: efd=%d, fd=%d, errorno=%d", m_epFd, fd, errno);
        return -1;
    }

    std::unique_lock lock(m_mapMutex);
    m_wakeDataMap[fd].emplace_back(std::move(wakeData));
    fdEmpty_.store(false);
    return 0;
}

int Poller::DelFdEvent(int fd) noexcept
{
    if (epoll_ctl(m_epFd, EPOLL_CTL_DEL, fd, nullptr) != 0) {
        FFRT_LOGE("epoll_ctl del fd error: efd=%d, fd=%d, errorno=%d", m_epFd, fd, errno);
        return -1;
    }

    std::unique_lock lock(m_mapMutex);
    m_delCntMap[fd]++;
    WakeUp();
    return 0;
}

void Poller::WakeUp() noexcept
{
    uint64_t one = 1;
    (void)::write(m_wakeData.fd, &one, sizeof one);
}

PollerRet Poller::PollOnce(int timeout) noexcept
{
    int realTimeout = timeout;
    int timerHandle = -1;

    timerMutex_.lock();
    if (!timerMap_.empty()) {
        auto cur = timerMap_.begin();
        timerHandle = cur->second.handle;

        realTimeout = std::chrono::duration_cast<std::chrono::milliseconds>(
            cur->first - std::chrono::steady_clock::now()).count();
        if (realTimeout <= 0) {
            ExecuteTimerCb(cur);
            return PollerRet::RET_TIMER;
        }

        if (timeout != -1) {
            timerHandle = -1;
            realTimeout = timeout;
        }

        flag_ = EpollStatus::WAIT;
    }
    timerMutex_.unlock();

    pollerCount_++;

    std::array<epoll_event, 1024> waitedEvents;
    int nfds = epoll_wait(m_epFd, waitedEvents.data(), waitedEvents.size(), realTimeout);
    flag_ = EpollStatus::WAKE;
    if (nfds < 0) {
        FFRT_LOGE("epoll_wait error.");
        return PollerRet::RET_NULL;
    }

    if (nfds == 0) {
        if (timerHandle != -1) {
            timerMutex_.lock();
            for (auto it = timerMap_.begin(); it != timerMap_.end(); it++) {
                if (it->second.handle == timerHandle) {
                    ExecuteTimerCb(it);
                    return PollerRet::RET_TIMER;
                }
            }
            timerMutex_.unlock();
        }
        return PollerRet::RET_NULL;
    }

    for (unsigned int i = 0; i < static_cast<unsigned int>(nfds); ++i) {
        struct WakeDataWithCb *data = reinterpret_cast<struct WakeDataWithCb *>(waitedEvents[i].data.ptr);
        int currFd = data->fd;
        if (currFd == m_wakeData.fd) {
            uint64_t one = 1;
            (void)::read(m_wakeData.fd, &one, sizeof one);
            continue;
        }

        if (data->cb == nullptr) {
            continue;
        }
        data->cb(data->data, waitedEvents[i].events);
    }

    ReleaseFdWakeData();
    return PollerRet::RET_EPOLL;
}

void Poller::ReleaseFdWakeData() noexcept
{
    std::unique_lock lock(m_mapMutex);
    for (auto delIter = m_delCntMap.begin(); delIter != m_delCntMap.end();) {
        int delFd = delIter->first;
        unsigned int delCnt = static_cast<unsigned int>(delIter->second);
        auto& wakeDataList = m_wakeDataMap[delFd];
        int diff = wakeDataList.size() - delCnt;
        if (diff == 0) {
            m_wakeDataMap.erase(delFd);
            m_delCntMap.erase(delIter++);
            continue;
        } else if (diff == 1) {
            for (unsigned int i = 0; i < delCnt - 1; i++) {
                wakeDataList.pop_front();
            }
            m_delCntMap[delFd] = 1;
        } else {
            FFRT_LOGE("fd=%d count unexpected, added num=%d, del num=%d", delFd, wakeDataList.size(), delCnt);
        }
        delIter++;
    }

    fdEmpty_.store(m_wakeDataMap.empty());
}

void Poller::ExecuteTimerCb(std::multimap<time_point_t, TimerDataWithCb>::iterator& timer) noexcept
{
    std::vector<TimerDataWithCb> timerData;
    for (auto iter = timerMap_.begin(); iter != timerMap_.end();) {
        if (iter->first <= timer->first) {
            timerData.emplace_back(iter->second);
            executedHandle_[iter->second.handle] = TimerStatus::EXECUTING;
            iter = timerMap_.erase(iter);
            continue;
        }
        break;
    }
    timerEmpty_.store(timerMap_.empty());

    timerMutex_.unlock();
    for (const auto& data : timerData) {
        data.cb(data.data);
        executedHandle_[data.handle] = TimerStatus::EXECUTED;
    }
}

int Poller::RegisterTimer(uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat) noexcept
{
    if (repeat) {
        FFRT_LOGE("repeat not supported yet");
        return -1;
    }
    if (cb == nullptr || flag_ == EpollStatus::TEARDOWN) {
        return -1;
    }

    std::lock_guard lock(timerMutex_);
    time_point_t absoluteTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    bool wake = timerMap_.empty() || (absoluteTime < timerMap_.begin()->first && flag_ == EpollStatus::WAIT);

    TimerDataWithCb timerMapValue(data, cb);
    timerHandle_ += 1;
    timerMapValue.handle = timerHandle_;
    timerMap_.emplace(absoluteTime, timerMapValue);
    timerEmpty_.store(false);

    if (wake) {
        WakeUp();
    }

    return timerHandle_;
}

int Poller::UnregisterTimer(int handle) noexcept
{
    if (flag_ == EpollStatus::TEARDOWN) {
        return -1;
    }

    std::lock_guard lock(timerMutex_);
    auto it = executedHandle_.find(handle);
    if (it != executedHandle_.end()) {
        while (it->second == TimerStatus::EXECUTING) {
            std::this_thread::yield();
        }
        executedHandle_.erase(it);
        return 0;
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

bool Poller::DetermineEmptyMap() noexcept
{
    return fdEmpty_ && timerEmpty_;
}

bool Poller::DeterminePollerReady() noexcept
{
    return IsFdExist() || IsTimerReady();
}

bool Poller::IsFdExist() noexcept
{
    return !fdEmpty_;
}

bool Poller::IsTimerReady() noexcept
{
    time_point_t now = std::chrono::steady_clock::now();
    std::lock_guard lock(timerMutex_);
    if (timerMap_.empty()) {
        return false;
    }

    if (now >= timerMap_.begin()->first) {
        return true;
    }
    return false;
}

ffrt_timer_query_t Poller::GetTimerStatus(int handle) noexcept
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
            std::this_thread::yield();
        }
        return ffrt_timer_executed;
    }

    return ffrt_timer_notfound;
}

uint8_t Poller::GetPollCount() noexcept
{
    return pollerCount_;
}
}