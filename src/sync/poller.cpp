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
#include "tm/scpu_task.h"
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
    m_waitTaskMap.clear();
}

PollerProxy* PollerProxy::Instance()
{
    static PollerProxy pollerInstance;
    return &pollerInstance;
}

int Poller::AddFdEvent(int op, uint32_t events, int fd, void* data, ffrt_poller_cb cb) noexcept
{
    auto wakeData = std::make_unique<WakeDataWithCb>(fd, data, cb, ExecuteCtx::Cur()->task);
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
    if (op == EPOLL_CTL_ADD) {
        m_wakeDataMap[fd].emplace_back(std::move(wakeData));
        fdEmpty_.store(false);
    } else if (op == EPOLL_CTL_MOD) {
        auto iter = m_wakeDataMap.find(fd);
        if (iter->second.size() < 1) {
            FFRT_LOGE("epoll_ctl mod fd wakedata num invalid\n");
            return -1;
        }
        iter->second.pop_back();
        iter->second.emplace_back(std::move(wakeData));
    }
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

int Poller::WaitFdEvent(struct epoll_event* eventsPtr, int maxevents, int timeout) noexcept
{
    if (eventsPtr == nullptr) {
        FFRT_LOGE("eventsPtr cannot be null");
        return -1;
    }

    auto task = ExecuteCtx::Cur()->task;
    if (!task) {
        FFRT_LOGI("nonworker shall not call this fun.");
        return -1;
    }

    if (maxevents < EPOLL_EVENT_SIZE) {
        FFRT_LOGE("maxEvents:%d cannot be less than 1024", maxevents);
        return -1;
    }

    bool legacyMode = LegacyMode(task);
    int nfds = 0;
    if (!USE_COROUTINE || legacyMode) {
        std::unique_lock<std::mutex> lck(task->lock);
        m_mapMutex.lock();
        if (m_waitTaskMap.find(task) != m_waitTaskMap.end()) {
            FFRT_LOGE("task has waited before");
            return -1;
        }
        if (legacyMode) {
            task->coRoutine->blockType = BlockType::BLOCK_THREAD;
        }
        auto currTime = std::chrono::steady_clock::now();
        m_waitTaskMap[task] = {(void*)eventsPtr, maxevents, &nfds, currTime};
        if (timeout > -1) {
            FFRT_LOGE("poller meet timeout={%d}", timeout);
            RegisterTimer(timeout, nullptr, nullptr);
        }
        m_mapMutex.unlock();
        reinterpret_cast<SCPUEUTask*>(task)->childWaitCond_.wait(lck);
        return 0;
    }

    CoWait([&](CPUEUTask *task)->bool {
        m_mapMutex.lock();
        if (m_waitTaskMap.find(task) != m_waitTaskMap.end()) {
            FFRT_LOGE("task has waited before");
            return false;
        }
        auto currTime = std::chrono::steady_clock::now();
        m_waitTaskMap[task] = {(void*)eventsPtr, maxevents, &nfds, currTime};
        if (timeout > -1) {
            FFRT_LOGE("poller meet timeout={%d}", timeout);
            RegisterTimer(timeout, nullptr, nullptr);
        }
        m_mapMutex.unlock();
        return true;
    });
    return 0;
}

void Poller::WakeUp() noexcept
{
    uint64_t one = 1;
    (void)::write(m_wakeData.fd, &one, sizeof one);
}

void Poller::ProcessWaitedFds(int nfds, std::unordered_map<CPUEUTask*, EventVec>& syncTaskEvents,
                              std::array<epoll_event, EPOLL_EVENT_SIZE>& waitedEvents) noexcept
{
    for (unsigned int i = 0; i < static_cast<unsigned int>(nfds); ++i) {
        struct WakeDataWithCb *data = reinterpret_cast<struct WakeDataWithCb *>(waitedEvents[i].data.ptr);
        int currFd = data->fd;
        if (currFd == m_wakeData.fd) {
            uint64_t one = 1;
            ssize_t n = ::read(m_wakeData.fd, &one, sizeof one);
            continue;
        }

        if (data->cb != nullptr) {
            data->cb(data->data, waitedEvents[i].events);
            continue;
        }

        if (data->task != nullptr) {
            epoll_event ev = { .events = waitedEvents[i].events, .data = {.fd = currFd} };
            syncTaskEvents[data->task].push_back(ev);
        }
    }
}

void Poller::WakeSyncTask(std::unordered_map<CPUEUTask*, EventVec>& syncTaskEvents) noexcept
{
    if (syncTaskEvents.empty()) {
        return;
    }

    m_mapMutex.lock();
    for (auto syncFditer = m_waitTaskMap.begin(); syncFditer != m_waitTaskMap.end();) {
        CPUEUTask* currTask = syncFditer->first;
        auto iter = syncTaskEvents.find(currTask);
        if (iter == syncTaskEvents.end()) {
            FFRT_LOGE(" event not coming, task=%lu, syncTaskEvents num=%u, m_waitTaskMap "
                      "num=%u", currTask->rank, syncTaskEvents.size(), m_waitTaskMap.size());
            syncFditer++;
            continue;
        }

        epoll_event* eventsPtr = (epoll_event*)syncFditer->second.eventsPtr;
        if (eventsPtr != nullptr) {
            int nfds = iter->second.size();
            for (int i = 0; i < nfds; i++) {
                eventsPtr[i].events = iter->second[i].events;
                eventsPtr[i].data.fd = iter->second[i].data.fd;
            }

            int* nfdsPtr = syncFditer->second.nfdsPtr;
            if (nfdsPtr) {
                *nfdsPtr = nfds;
            }
        }
        m_waitTaskMap.erase(syncFditer++);

        bool blockThread = BlockThread(currTask);
        if (!USE_COROUTINE || blockThread) {
            std::unique_lock<std::mutex> lck(currTask->lock);
            if (blockThread) {
                currTask->coRoutine->blockType = BlockType::BLOCK_COROUTINE;
            }
            reinterpret_cast<SCPUEUTask*>(currTask)->childWaitCond_.notify_one();
        } else {
            CoWake(currTask, false);
        }
        syncTaskEvents.erase(iter);
    }
    m_mapMutex.unlock();
}

uint64_t Poller::GetTaskWaitTime(CPUEUTask* task) noexcept
{
    std::unique_lock lock(m_mapMutex);
    auto iter = m_waitTaskMap.find(task);
    if (iter == m_waitTaskMap.end()) {
        return 0;
    }

    return std::chrono::duration_cast<std::chrono::seconds>(
        iter->second.waitTP.time_since_epoch()).count();
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
            ExecuteTimerCb(cur->first);
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

    std::array<epoll_event, EPOLL_EVENT_SIZE> waitedEvents;
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
                    ExecuteTimerCb(it->first);
                    return PollerRet::RET_TIMER;
                }
            }
            timerMutex_.unlock();
        }
        return PollerRet::RET_NULL;
    }

    std::unordered_map<CPUEUTask*, EventVec> syncTaskEvents;
    ProcessWaitedFds(nfds, syncTaskEvents, waitedEvents);
    WakeSyncTask(syncTaskEvents);

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

void Poller::ProcessTimerDataCb(CPUEUTask* task) noexcept
{
    m_mapMutex.lock();
    auto iter = m_waitTaskMap.find(task);
    if (iter != m_waitTaskMap.end()) {
        bool blockThread = BlockThread(task);
        if (!USE_COROUTINE || blockThread) {
            std::unique_lock<std::mutex> lck(task->lock);
            if (blockThread) {
                task->coRoutine->blockType = BlockType::BLOCK_COROUTINE;
            }
            reinterpret_cast<SCPUEUTask*>(task)->childWaitCond_.notify_one();
        } else {
            CoWake(task, false);
        }
        m_waitTaskMap.erase(iter);
    }
    m_mapMutex.unlock();
}

void Poller::ExecuteTimerCb(time_point_t timer) noexcept
{
    std::vector<TimerDataWithCb> timerData;
    for (auto iter = timerMap_.begin(); iter != timerMap_.end();) {
        if (iter->first <= timer) {
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
        if (data.cb) {
            data.cb(data.data);
        } else if (data.task != nullptr) {
            ProcessTimerDataCb(data.task);
        }
        executedHandle_[data.handle] = TimerStatus::EXECUTED;
    }
}

int Poller::RegisterTimer(uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat) noexcept
{
    if (repeat) {
        FFRT_LOGE("repeat not supported yet");
        return -1;
    }
    if (flag_ == EpollStatus::TEARDOWN) {
        return -1;
    }

    std::lock_guard lock(timerMutex_);
    time_point_t absoluteTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    bool wake = timerMap_.empty() || (absoluteTime < timerMap_.begin()->first && flag_ == EpollStatus::WAIT);

    TimerDataWithCb timerMapValue(data, cb, ExecuteCtx::Cur()->task);
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