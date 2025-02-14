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
#include <sstream>
#include <sys/prctl.h>
#include <sys/timerfd.h>
#include <thread>
#include <pthread.h>
#include "eu/blockaware.h"
#include "eu/execute_unit.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/assert.h"
#include "util/name_manager.h"
#include "sched/scheduler.h"
#include "util/ffrt_facade.h"
namespace {
const uintptr_t FFRT_DELAY_WORKER_MAGICNUM = 0x5aa5;
const int FFRT_DELAY_WORKER_IDLE_TIMEOUT_SECONDS = 3 * 60;
const int EPOLL_WAIT_TIMEOUT__MILISECONDS = 3 * 60 * 1000;
const int NS_PER_SEC = 1000 * 1000 * 1000;
const int FAKE_WAKE_UP_ERROR = 4;
const int WAIT_EVENT_SIZE = 5;
const int64_t EXECUTION_TIMEOUT_MILISECONDS = 500;
const int DUMP_MAP_MAX_COUNT = 3;
constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
constexpr int ASYNC_TASK_SLEEP_MS = 1;
}

namespace ffrt {
pthread_key_t g_ffrtDelayWorkerFlagKey;
pthread_once_t g_ffrtDelayWorkerThreadKeyOnce = PTHREAD_ONCE_INIT;
void FFRTDelayWorkeEnvKeyCreate()
{
    pthread_key_create(&g_ffrtDelayWorkerFlagKey, nullptr);
}

void DelayedWorker::ThreadEnvCreate()
{
    pthread_once(&g_ffrtDelayWorkerThreadKeyOnce, FFRTDelayWorkeEnvKeyCreate);
}

bool DelayedWorker::IsDelayerWorkerThread()
{
    bool isDelayerWorkerFlag = false;
    void* flag = pthread_getspecific(g_ffrtDelayWorkerFlagKey);
    if ((flag != nullptr) && (reinterpret_cast<uintptr_t>(flag) == FFRT_DELAY_WORKER_MAGICNUM)) {
        isDelayerWorkerFlag = true;
    }
    return isDelayerWorkerFlag;
}

bool IsDelayedWorkerPreserved()
{
    std::unordered_set<std::string> whitelist = { "foundation", "com.ohos.sceneboard" };
    char processName[PROCESS_NAME_BUFFER_LENGTH];
    GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
    if (whitelist.find(processName) != whitelist.end()) {
        return true;
    }

    return false;
}

void DelayedWorker::DumpMap()
{
    lock.lock();
    if (map.empty()) {
        lock.unlock();
        return;
    }

    TimePoint now = std::chrono::steady_clock::now();
    if (now < map.begin()->first) {
        lock.unlock();
        return;
    }

    int count = 0;
    std::stringstream ss;
    int printCount = map.size() < DUMP_MAP_MAX_COUNT ? map.size() : DUMP_MAP_MAX_COUNT;
    for (auto it = map.begin(); it != map.end() && count < DUMP_MAP_MAX_COUNT; ++it, ++count) {
        ss << it->first.time_since_epoch().count();
        if (count < printCount - 1) {
            ss << ",";
        }
    }
    lock.unlock();
    FFRT_LOGW("DumpMap:now=%lu,%s", now.time_since_epoch().count(), ss.str().c_str());
}

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
        } else {
            FFRT_LOGW("delayedWorker init");
        }
        prctl(PR_SET_NAME, DELAYED_WORKER_NAME);
        pthread_setspecific(g_ffrtDelayWorkerFlagKey, reinterpret_cast<void*>(FFRT_DELAY_WORKER_MAGICNUM));
        std::array<epoll_event, WAIT_EVENT_SIZE> waitedEvents;
        static bool preserved = IsDelayedWorkerPreserved();
        for (;;) {
            std::unique_lock lk(lock);
            if (toExit) {
                exited_ = true;
                FFRT_LOGW("delayedWorker exit");
                break;
            }
            int result = HandleWork();
            if (toExit) {
                exited_ = true;
                FFRT_LOGW("delayedWorker exit");
                break;
            }
            if (result == 0) {
                uint64_t ns = map.begin()->first.time_since_epoch().count();
                itimerspec its = { {0, 0}, {static_cast<long>(ns / NS_PER_SEC), static_cast<long>(ns % NS_PER_SEC)} };
                ret = timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &its, nullptr);
                if (ret != 0) {
                    FFRT_LOGE("timerfd_settime error,ns=%lu,ret= %d.", ns, ret);
                }
            } else if ((result == 1) && (!preserved)) {
                if (++noTaskDelayCount_ > 1) {
                    exited_ = true;
                    FFRT_LOGW("delayedWorker exit");
                    break;
                }
                itimerspec its = { {0, 0}, {FFRT_DELAY_WORKER_IDLE_TIMEOUT_SECONDS, 0} };
                ret = timerfd_settime(timerfd_, 0, &its, nullptr);
                if (ret != 0) {
                    FFRT_LOGE("timerfd_settime error, ret= %d.", ret);
                }
            }
            lk.unlock();

            int nfds = epoll_wait(epollfd_, waitedEvents.data(), waitedEvents.size(),
                EPOLL_WAIT_TIMEOUT__MILISECONDS);
            if (nfds == 0) {
                DumpMap();
            }
            FFRT_TRACE_END();

            if (nfds < 0) {
                if (errno != FAKE_WAKE_UP_ERROR) {
                    FFRT_LOGW("epoll_wait error, errorno= %d.", errno);
                }
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
    monitor = ExecuteUnit::Instance().GetCPUMonitor();
    monitorfd_ = BlockawareMonitorfd(-1, monitor->WakeupCond());
    FFRT_ASSERT(monitorfd_ >= 0);
    FFRT_LOGI("timerfd:%d, monitorfd:%d", timerfd_, monitorfd_);
    /* monitorfd does not support 'CLOEXEC', and current kernel does not inherit monitorfd after 'fork'.
     * 1. if user calls 'exec' directly after 'fork' and does not use ffrt, it's ok.
     * 2. if user calls 'exec' directly, the original process cannot close monitorfd automatically, and
     * it will be fail when new program use ffrt to create monitorfd.
     */
    epoll_event monitor_event {.events = EPOLLIN, .data = {.fd = monitorfd_}};
    int ret = epoll_ctl(epollfd_, EPOLL_CTL_ADD, monitorfd_, &monitor_event);
    if (ret < 0) {
        FFRT_LOGE("monitor:%d add fail, ret:%d, errno:%d, %s", monitorfd_, ret, errno, strerror(errno));
    }
#endif
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
    while (asyncTaskCnt_.load() > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(ASYNC_TASK_SLEEP_MS));
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

void CheckTimeInterval(const TimePoint& startTp, const TimePoint& endTp)
{
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTp - startTp);
    int64_t durationMs = duration.count();
    if (durationMs > EXECUTION_TIMEOUT_MILISECONDS) {
        FFRT_LOGW("handle work more than [%lld]ms", durationMs);
    }
}

int DelayedWorker::HandleWork()
{
    if (!map.empty()) {
        noTaskDelayCount_ = 0;
        TimePoint startTp = std::chrono::steady_clock::now();
        do {
            auto cur = map.begin();
            if (!toExit && cur != map.end() && cur->first <= startTp) {
                DelayedWork w = cur->second;
                map.erase(cur);
                lock.unlock();
                std::function<void(WaitEntry*)> workCb(move(*w.cb));
                (workCb)(w.we);
                lock.lock();
                FFRT_COND_DO_ERR(toExit, return -1, "HandleWork exit, map size:%d", map.size());
                TimePoint endTp = std::chrono::steady_clock::now();
                CheckTimeInterval(startTp, endTp);
                startTp = std::move(endTp);
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
        uint64_t ns = static_cast<uint64_t>(to.time_since_epoch().count());
        itimerspec its = { {0, 0}, {static_cast<long>(ns / NS_PER_SEC), static_cast<long>(ns % NS_PER_SEC)} };
        int ret = timerfd_settime(timerfd_, TFD_TIMER_ABSTIME, &its, nullptr);
        if (ret != 0) {
            FFRT_LOGE("timerfd_settime error, ns=%lu, ret= %d.", ns, ret);
        }
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

void DelayedWorker::SubmitAsyncTask(std::function<void()>&& func)
{
    asyncTaskCnt_.fetch_add(1);
    ffrt::submit([this, func = std::move(func)]() {
        if (toExit) {
            asyncTaskCnt_.fetch_sub(1);
            return;
        }

        func();
        asyncTaskCnt_.fetch_sub(1);
        }, {}, {this},
            ffrt::task_attr().qos(qos_background));
}
} // namespace ffrt