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

#ifndef HICORO_POLLER_H
#define HICORO_POLLER_H
#ifndef _MSC_VER
#include <sys/epoll.h>
#include <sys/eventfd.h>
#endif
#include <list>
#include <unordered_map>
#include <array>
#include "qos.h"
#include "sync/sync.h"
#include "internal_inc/non_copyable.h"
#include "c/executor_task.h"
#include "c/timer.h"
#include "eu/worker_thread.h"
namespace ffrt {
enum class PollerRet {
    RET_NULL,
    RET_EPOLL,
    RET_TIMER,
};

enum class EpollStatus {
    WAIT,
    WAKE,
    TEARDOWN,
};

enum class TimerStatus {
    EXECUTING,
    EXECUTED,
};

constexpr int EPOLL_EVENT_SIZE = 1024;

struct WakeDataWithCb {
    WakeDataWithCb() {}
    WakeDataWithCb(int fdVal, void *dataVal, std::function<void(void *, uint32_t)> cbVal, CPUEUTask *taskVal)
        : fd(fdVal), data(dataVal), cb(cbVal), task(taskVal)
    {}

    int fd = 0;
    void* data = nullptr;
    std::function<void(void*, uint32_t)> cb = nullptr;
    CPUEUTask* task = nullptr;
};

struct TimerDataWithCb {
    TimerDataWithCb() {}
    TimerDataWithCb(void *dataVal, void (*cbVal)(void *), CPUEUTask *taskVal) : data(dataVal), cb(cbVal), task(taskVal)
    {}

    void* data = nullptr;
    void(*cb)(void*) = nullptr;
    int handle = -1;
    CPUEUTask* task = nullptr;
};

struct SyncData {
    SyncData() {}
    SyncData(void *eventsPtr, int maxEvents, int *nfdsPtr, time_point_t waitTP)
        : eventsPtr(eventsPtr), maxEvents(maxEvents), nfdsPtr(nfdsPtr), waitTP(waitTP)
    {}

    void* eventsPtr = nullptr;
    int maxEvents = 0;
    int* nfdsPtr = nullptr;
    time_point_t waitTP;
};

class Poller : private NonCopyable {
    using WakeDataList = typename std::list<std::unique_ptr<struct WakeDataWithCb>>;
    using EventVec = typename std::vector<epoll_event>;
public:
    Poller() noexcept;
    ~Poller() noexcept;

    int AddFdEvent(int op, uint32_t events, int fd, void* data, ffrt_poller_cb cb) noexcept;
    int DelFdEvent(int fd) noexcept;
    int WaitFdEvent(struct epoll_event *eventsVec, int maxevents, int timeout) noexcept;

    PollerRet PollOnce(int timeout = -1) noexcept;
    void WakeUp() noexcept;

    int RegisterTimer(uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat = false) noexcept;
    int UnregisterTimer(int handle) noexcept;
    ffrt_timer_query_t GetTimerStatus(int handle) noexcept;

    uint8_t GetPollCount() noexcept;

    bool GetTaskWaitStatus(CPUEUTask* task) noexcept;
    uint64_t GetTaskWaitTime(CPUEUTask* task) noexcept;

    bool DetermineEmptyMap() noexcept;
    bool DeterminePollerReady() noexcept;

private:
    void ReleaseFdWakeData() noexcept;
    void WakeSyncTask(std::unordered_map<CPUEUTask*, EventVec>& syncTaskEvents) noexcept;
    void ProcessWaitedFds(int nfds, std::unordered_map<CPUEUTask*, EventVec>& syncTaskEvents,
                          std::array<epoll_event, EPOLL_EVENT_SIZE>& waitedEvents) noexcept;
    void ExecuteTimerCb(time_point_t timer) noexcept;
    void ProcessTimerDataCb(CPUEUTask* task) noexcept;
    bool IsFdExist() noexcept;
    bool IsTimerReady() noexcept;

    int m_epFd;
    uint8_t pollerCount_ = 0;
    int timerHandle_ = -1;
    EpollStatus flag_ = EpollStatus::WAKE;
    struct WakeDataWithCb m_wakeData;
    std::unordered_map<int, WakeDataList> m_wakeDataMap;
    std::unordered_map<int, int> m_delCntMap;
    std::unordered_map<CPUEUTask*, SyncData> m_waitTaskMap;

    std::unordered_map<int, TimerStatus> executedHandle_;
    std::multimap<time_point_t, TimerDataWithCb> timerMap_;
    std::atomic_bool fdEmpty_ {true};
    std::atomic_bool timerEmpty_ {true};
    mutable spin_mutex m_mapMutex;
    mutable spin_mutex timerMutex_;
};

struct PollerProxy {
public:
    static PollerProxy* Instance();

    Poller& GetPoller(const QoS& qos = QoS(ffrt_qos_default))
    {
        return qosPollers[static_cast<size_t>(qos())];
    }

private:
    std::array<Poller, QoS::MaxNum()> qosPollers;
};
} // namespace ffrt
#endif