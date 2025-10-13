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

#ifndef FFRT_LOOP_POLLER_MANAGER_H
#define FFRT_LOOP_POLLER_MANAGER_H
#include "eu/base_poller.h"

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

struct TimerDataWithCb {
    TimerDataWithCb() {}
    TimerDataWithCb(void* dataVal, std::function<void(void*)> cbVal, CoTask* taskVal, bool repeat, uint64_t timeout)
        : data(dataVal), cb(cbVal), task(taskVal), repeat(repeat), timeout(timeout)
    {
        if (cb != nullptr) {
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (TraceChainAdapter::Instance().HiTraceChainGetId().valid == HITRACE_ID_VALID) {
                traceId = TraceChainAdapter::Instance().HiTraceChainCreateSpan();
            };
#endif
        }
    }

    void* data = nullptr;
    std::function<void(void*)> cb = nullptr;
    int handle = -1;
    CoTask* task = nullptr;
    bool repeat = false;
    uint64_t timeout = 0;
    HiTraceIdStruct traceId;
};

class LoopPoller : public BasePoller {
public:
    ~LoopPoller() noexcept override;

    PollerRet PollOnce(int timeout = -1) noexcept;

    int RegisterTimer(uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat = false) noexcept;
    int UnregisterTimer(int handle) noexcept;
    ffrt_timer_query_t GetTimerStatus(int handle) noexcept;

    bool DetermineEmptyMap() noexcept;
    bool DeterminePollerReady() noexcept;

    bool IsIOPoller() override { return false; }
    void Run() override {}
    inline int PollerRegisterTimer(int qos, uint64_t timeout, void* data) override
    {
        (void)qos;
        (void)data;
        return RegisterTimer(timeout, nullptr, nullptr);
    }

    void PollerUnRegisterTimer(std::unordered_set<int>& timerHandlesToRemove) override
    {
        if (timerHandlesToRemove.size() > 0) {
            std::lock_guard lock(timerMutex_);
            for (auto it = timerMap_.begin(); it != timerMap_.end();) {
                if (timerHandlesToRemove.find(it->second.handle) != timerHandlesToRemove.end()) {
                    it = timerMap_.erase(it);
                } else {
                    ++it;
                }
            }
            timerEmpty_.store(timerMap_.empty());
        }
    }

private:
    void ProcessWaitedFds(int nfds, std::unordered_map<CoTask*, EventVec>& syncTaskEvents,
                          std::array<epoll_event, EPOLL_EVENT_SIZE>& waitedEvents) noexcept;

    void ExecuteTimerCb(TimePoint timer) noexcept;
    void ProcessTimerDataCb(CoTask* task) noexcept;
    void RegisterTimerImpl(const TimerDataWithCb& data) noexcept;
    PollerRet FindAndExecuteTimer(int timerHandle);

    bool IsFdExist() noexcept;
    bool IsTimerReady() noexcept;

    int timerHandle_ = -1;
    std::atomic<EpollStatus> flag_ = EpollStatus::WAKE;

    std::unordered_map<int, TimerStatus> executedHandle_;
    std::multimap<TimePoint, TimerDataWithCb> timerMap_;
    std::atomic_bool timerEmpty_ {true};
    mutable fast_mutex timerMutex_;
};

struct PollerProxy {
public:
    static PollerProxy& Instance();

    LoopPoller& GetPoller(const QoS& qos = QoS(ffrt_qos_default))
    {
        return qosPollers[static_cast<size_t>(qos())];
    }

private:
    std::array<LoopPoller, QoS::MaxNum()> qosPollers;
};
} // namespace ffrt
#endif