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

#include "delayed_worker.h"

#include <array>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/timerfd.h>
#include <thread>
// #include "eu/execute_unit.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/assert.h"
#include "util/name_manager.h"
// #include "sched/scheduler.h"
namespace {
    const int FFRT_DELAY_WORKER_IDLE_TIMEOUT_SECONDS = 3 * 60;
    const int NS_PER_SEC = 1000 * 1000 * 1000;
    const int WAIT_EVENT_SIZE = 5;
}
namespace ffrt {
void DelayedWorker::ThreadInit()
{
    if (delayWorker != nullptr && delayWorker->joinable()) {
        delayWorker->join();
    }
    delayWorker = std::make_unique<std::thread>([this]() {
        struct sched_param param;
        param.sched_priority = 1;
        int ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
        if (ret != 0) {
            FFRT_LOGW("[%d] set priority warn ret[%d] eno[%d]\n", pthread_self(), ret, errno);
        }
        prctl(PR_SET_NAME, DELAYED_WORKER_NAME);
        std::array<epoll_event, WAIT_EVENT_SIZE> waitedEvents;
        for (;;) {
            std::unique_lock lk(lock);
            if (toExit) {
                exited_ = true;
                break;
            }
            int result = HandleWork();
            if (toExit) {
                exited_ = true;
                break;
            }
            if (result == 0) {
                uint64_t ns = map.begin()->first.time_since_epoch().count();
                itimerspec its = { {0, 0}, {static_cast<long>(ns / NS_PER_SEC), static_cast<long>(ns % NS_PER_SEC)} };
                timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &its, nullptr);
            } else if (result == 1) {
                if (++noTaskDelayCount_ > 1) {
                    exited_ = true;
                    break;
                }
                itimerspec its = { {0, 0}, {FFRT_DELAY_WORKER_IDLE_TIMEOUT_SECONDS, 0} };
                timerfd_settime(timerfd_, 0, &its, nullptr);
            }
            lk.unlock();
            int nfds = epoll_wait(epollfd_, waitedEvents.data(), waitedEvents.size(), -1);
            if (nfds < 0) {
                FFRT_LOGE("epoll_wait error, errorno= %d.", errno);
                continue;
            }
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
            for (int i = 0; i < nfds; i++) {
                if (waitedEvents[i].data.fd == monitorfd_) {
                    char buffer;
                    size_t n = ::read(monitorfd_, &buffer, sizeof buffer);
                    if (n == 1) {
                        monitor->MonitorMain();
                    } else {
                        FFRT_LOGE("monitor read fail:%d, %s", n, errno);
                    }
                    break;
                }
            }
#endif
        }
    });
}

DelayedWorker::DelayedWorker(): epollfd_ { ::epoll_create1(EPOLL_CLOEXEC) },
    timerfd_ { ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC) }
{
    FFRT_ASSERT(epollfd_ >= 0);
    FFRT_ASSERT(timerfd_ >= 0);

    epoll_event timer_event { .events = EPOLLIN | EPOLLET, .data = { .fd = timerfd_ } };
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, timerfd_, &timer_event) < 0) {
        FFRT_LOGE("epoll_ctl add tfd error: efd=%d, fd=%d, errorno=%d", epollfd_, timerfd_, errno);
        std::terminate();
    }
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    FFRTScheduler::Instance();
    monitor = ExecuteUnit::Instance().GetCPUMonitor();
    monitorfd_ = BlockawareMonitorfd(-1, monitor->WakeupCond());
    FFRT_ASSERT(monitorfd_ >= 0);
    epoll_event monitor_event {.event = EPOLLIN, .data = {.fd = monitorfd_}};
    int ret = epoll_ctl(epollfd_, EPOLL_CTL_ADD, monitorfd_, &monitor_event);
    if (ret < 0) {
        FFRT_LOGE("monitor:%d add fail, ret:%d, errno:%d, %s", monitorfd_, ret, errno, strerror(errno));
    }
#endif
    ThreadInit();
}

DelayedWorker::~DelayedWorker()
{
    lock.lock();
    toExit = true;
    lock.unlock();
    itimerspec its = { {0, 0}, {0, 1} };
    timerfd_settime(timerfd_, 0, &its, nullptr);
    if (delayWorker != nullptr && delayWorker->joinable()) {
        delayWorker->join();
    }
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ::close(monitorfd_);
#endif
    ::close(timerfd_);
}

DelayedWorker& DelayedWorker::GetInstance()
{
    static DelayedWorker instance;
    return instance;
}

int DelayedWorker::HandleWork()
{
    if (!map.empty()) {
        noTaskDelayCount_ = 0;
        do {
            TimePoint now = std::chrono::steady_clock::now();
            auto cur = map.begin();
            if (!toExit && cur != map.end() && cur->first <= now) {
                DelayedWork w = cur->second;
                map.erase(cur);
                lock.unlock();
                (*w.cb)(w.we);
                lock.lock();
                FFRT_COND_DO_ERR(toExit, return -1, "HandleWork exit, map size:%d", map.size());
            } else {
                return 0;
            }
        } while (!map.empty());
    }
    return 1;
}

// There is no requirement that to be less than now
bool DelayedWorker::dispatch(const TimePoint& to, WaitEntry* we, const std::function<void(WaitEntry*)>& wakeup)
{
    bool w = false;
    lock.lock();
    if (toExit) {
        lock.unlock();
        FFRT_LOGE("DelayedWorker destroy, dispatch failed\n");
        return false;
    }

    TimePoint now = std::chrono::steady_clock::now();
    if (to <= now) {
        lock.unlock();
        return false;
    }

    if (exited_) {
        ThreadInit();
        exited_ = false;
    }

    if (map.empty() || to < map.begin()->first) {
        w = true;
    }
    map.emplace(to, DelayedWork {we, &wakeup});
    if (w) {
        uint64_t ns = to.time_since_epoch().count();
        itimerspec its = { {0, 0}, {static_cast<long>(ns / NS_PER_SEC), static_cast<long>(ns % NS_PER_SEC)} };
        timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &its, nullptr);
    }
    lock.unlock();
    return true;
}

bool DelayedWorker::remove(const TimePoint& to, WaitEntry* we)
{
    std::lock_guard<decltype(lock)> l(lock);

    auto range = map.equal_range(to);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.we == we) {
            map.erase(it);
            return true;
        }
    }

    return false;
}
} // namespace ffrt