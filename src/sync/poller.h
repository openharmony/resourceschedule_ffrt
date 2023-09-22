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
#include "sched/qos.h"
#include "sync/sync.h"
#ifdef FFRT_IO_TASK_SCHEDULER
#include <list>
#include <unordered_map>
#include <array>
#include "internal_inc/non_copyable.h"
namespace ffrt {
struct WakeDataWithCb {
    WakeDataWithCb()
    {}
    WakeDataWithCb(int fdVal, void *dataVal, std::function<void(void *, uint32_t)> cbVal)
        : fd(fdVal), data(dataVal), cb(cbVal)
    {}

    int fd = 0;
    void *data = nullptr;
    std::function<void(void *, uint32_t)> cb;
};

enum class PollerRet {
    RET_NULL,
    RET_EPOLL,
    RET_TIMER,
};

class Poller : private NonCopyable {
    using WakeDataList = typename std::list<std::unique_ptr<struct WakeDataWithCb>>;
public:
    Poller() noexcept;
    ~Poller() noexcept;

    int AddFdEvent(uint32_t events, int fd, void* data, void(*cb)(void*, uint32_t)) noexcept;
    int DelFdEvent(int fd) noexcept;

    PollerRet PollOnce(int timeout = -1) noexcept;
    void WakeUp() noexcept;

    bool RegisterTimerFunc(int(*timerFunc)()) noexcept;

private:
    void ReleaseFdWakeData(int fd) noexcept;

    int m_epFd;
    struct WakeDataWithCb m_wakeData;
    std::unordered_map<int, WakeDataList> m_wakeDataMap;
    std::unordered_map<int, int> m_delCntMap;
    mutable spin_mutex m_mapMutex;
    std::function<int()> m_timerFunc;
#ifndef _MSC_VER
    std::vector<epoll_event> m_events;
#endif
};

struct PollerProxy {
public:
    static inline PollerProxy* Instance()
    {
        static PollerProxy pollerInstance;
        return &pollerInstance;
    }

    Poller& GetPoller(const QoS& qos = ffrt_qos_default)
    {
        return qosPollers[static_cast<size_t>(qos)];
    }

private:
    std::array<Poller, QoS::Max()> qosPollers;
};
} // namespace ffrt
#endif
#endif