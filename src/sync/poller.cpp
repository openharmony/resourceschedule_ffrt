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
#include <cassert>

#include "sched/execute_ctx.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
Poller::Poller() noexcept: m_epFd { ::epoll_create1(EPOLL_CLOEXEC) },
    m_events(1024)
{
    assert(m_epFd >= 0);
    {
        m_wakeData.cb = nullptr;
        m_wakeData.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        assert(m_wakeData.fd >= 0);
        epoll_event ev{ .events = EPOLLIN, .data = { .ptr = static_cast<void*>(&m_wakeData)}};
        if (epoll_ctl(m_epFd, EPOLL_CTL_ADD, m_wakeData.fd, &ev) < 0) {
            std::terminate();
        }
    }
}

Poller::~Poller() noexcept
{
    ::close(m_wakeData.fd);
    ::close(m_epFd);
    m_wakeDataMap.clear();
    m_delCntMap.clear();
}

int Poller::AddFdEvent(uint32_t events, int fd, void* data, void(*cb)(void*, uint32_t)) noexcept
{
    auto wakeData = std::unique_ptr<WakeDataWithCb>(new (std::nothrow) WakeDataWithCb(fd, data, cb));
    epoll_event ev = {.events = events, .data = {.ptr = static_cast<void*>(wakeData.get())}};

    if (epoll_ctl(m_epFd, EPOLL_CTL_ADD, fd, &ev) != 0) {
        FFRT_LOGE("epoll_ctl add fd error: efd = %d, fd = %d, errorno = %d", m_epFd, fd, errno);
        return -1;
    }

    std::unique_lock lock(m_mapMutex);
    m_wakeDataMap[fd].emplace_back(std::move(wakeData));
    return 0;
}

int Poller::DelFdEvent(int fd) noexcept
{
    if (epoll_ctl(m_epFd, EPOLL_CTL_DEL, fd, nullptr) != 0) {
        FFRT_LOGE("epoll_ctl del fd error: efd = %d, fd = %d, errorno = %d", m_epFd, fd, errno);
        return -1;
    }
    std::unique_lock lock(m_mapMutex);
    m_delCntMap[fd]++;
    return 0;
}

void Poller::ReleaseFdWakeData(int fd) noexcept
{
    std::unique_lock lock(m_mapMutex);
    if (m_delCntMap[fd] > 0) {
        auto& wakeDataList = m_wakeDataMap[fd];
        int diff = wakeDataList.size() - m_delCntMap[fd];
        if (diff == 0) {
            m_wakeDataMap.erase(fd);
            m_delCntMap[fd] = 0;
        } else if (diff == 1) {
            while (m_delCntMap[fd] > 0) {
                wakeDataList.pop_front();
                m_delCntMap[fd]--;
            }
        } else {
            FFRT_LOGE("fd = %d count unexpected, added num = %d, del num = %d",
                fd, wakeDataList.size(), m_delCntMap[fd]);
        }
    }
}

PollerRet Poller::PollOnce(int timeout) noexcept
{
    int realTimeout = timeout;
    PollerRet ret = PollerRet::RET_NULL;
    if (m_timerFunc ! = nullptr) {
        int nextTimeout = m_timerFunc();
        if (nextTimeout == 0) {
            return PollerRet::RET_TIMER;
        }
        if (timeout == -1 && nextTimeout > 0) {
            realTimeout = nextTimeout;
            ret = PollerRet::RET_TIMER;
        }
    }
    int nfds = epoll_wait(m_epFd, m_events.data(), m_events.size(), realTimeout);
    if (nfds <= 0) {
        if (realTimeout > 0) {
            int nextTimeout = m_timerFunc();
        }
        return ret;
    }

    for (unsigned int i = 0; i < static_cast<unsigned int>(nfds); ++i) {
        struct WakeDataWithCb *data = reinterpret_cast<struct WakeDataWithCb *>(m_events[i].data.ptr);
        int currFd = data->fd;
        if (currFd == m_wakeData.fd) {
            uint64_t one = 1;
            ssize_t n = ::read(m_wakeData.fd, &one, sizeof one);
            assert(n == sizeof one);
            continue;
        }

        if (data->cb == nullptr) {
            continue;
        }
        data->cb(data->data, m_events[i].events);
        ReleaseFdWakeData(currFd);
    }
    return PollerRet::RET_EPOLL;
}

void Poller::WakeUp() noexcept
{
    uint64_t one = 1;
    ssize_t n = ::write(m_wakeData.fd, &one, sizeof one);
    assert(n == sizeof one);
}

bool Poller::RegisterTimerFunc(int(*timerFunc)()) noexcept
{
    if (m_timerFunc == nullptr) {
        m_timerFunc == timerFunc;
        return true;
    }
    return false;
}
}