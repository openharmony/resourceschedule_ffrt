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

#ifndef FFRT_BASE_POLLER_MANAGER_H
#define FFRT_BASE_POLLER_MANAGER_H
#ifndef _MSC_VER
#include <sys/epoll.h>
#include <sys/eventfd.h>
#endif
#include <list>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <array>
#ifdef USE_OHOS_QOS
#include "qos.h"
#else
#include "staging_qos/sched/qos.h"
#endif
#include "sync/sync.h"
#include "tm/task_base.h"
#include "internal_inc/non_copyable.h"
#include "c/executor_task.h"
#include "c/timer.h"
#ifdef FFRT_ENABLE_HITRACE_CHAIN
#include "dfx/trace/ffrt_trace_chain.h"
#endif

namespace ffrt {
enum class PollerType {
    WAKEUP,
    SYNC_IO,
    ASYNC_CB,
    ASYNC_IO,
};

struct PollerData {
    PollerData() {}
    PollerData(int fdVal, CoTask* taskVal) : fd(fdVal), task(taskVal)
    {
        mode = PollerType::SYNC_IO;
    }
    PollerData(int fdVal, void* dataVal, std::function<void(void*, uint32_t)> cbVal, CoTask* taskVal)
        : fd(fdVal), data(dataVal), cb(cbVal), task(taskVal)
    {
        if (cb == nullptr) {
            mode = PollerType::ASYNC_IO;
        } else {
            mode = PollerType::ASYNC_CB;
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (TraceChainAdapter::Instance().HiTraceChainGetId().valid == HITRACE_ID_VALID) {
                traceId = TraceChainAdapter::Instance().HiTraceChainCreateSpan();
            };
#endif
        }
    }

    PollerType mode;
    int fd = 0;
    void* data = nullptr;
    std::function<void(void*, uint32_t)> cb = nullptr;
    CoTask* task = nullptr;
    uint32_t monitorEvents = 0;
    HiTraceIdStruct traceId;
};

struct SyncData {
    SyncData() {}
    SyncData(void* eventsPtr, int maxEvents, int* nfdsPtr, TimePoint waitTP)
        : eventsPtr(eventsPtr), maxEvents(maxEvents), nfdsPtr(nfdsPtr), waitTP(waitTP)
    {}

    void* eventsPtr = nullptr;
    int maxEvents = 0;
    int* nfdsPtr = nullptr;
    TimePoint waitTP;
    int timerHandle = -1;
};

using EventVec = typename std::vector<epoll_event>;
class BasePoller : private NonCopyable {
    using WakeDataList = typename std::list<std::unique_ptr<struct PollerData>>;
public:
    static constexpr int EPOLL_EVENT_SIZE = 1024;
    BasePoller() noexcept;
    virtual ~BasePoller() noexcept;

    int AddFdEvent(int op, uint32_t events, int fd, void* data, ffrt_poller_cb cb) noexcept;
    int DelFdEvent(int fd) noexcept;
    int WaitFdEvent(struct epoll_event* eventsVec, int maxevents, int timeout) noexcept;

    void WakeUp() noexcept;

    inline uint64_t GetPollCount() noexcept { return pollerCount_; }

    inline uint64_t GetTaskWaitTime(CoTask* task) noexcept
    {
        std::unique_lock lock(mapMutex_);
        auto iter = waitTaskMap_.find(task);
        if (iter == waitTaskMap_.end()) {
            return 0;
        }
        return std::chrono::duration_cast<std::chrono::seconds>(
            iter->second.waitTP.time_since_epoch()).count();
    }

    inline void ClearCachedEvents(CoTask* task) noexcept
    {
        std::unique_lock lock(mapMutex_);
        auto iter = cachedTaskEvents_.find(task);
        if (iter == cachedTaskEvents_.end()) {
            return;
        }
        cachedTaskEvents_.erase(iter);
        ClearMaskWakeDataCache(task);
    }

    void ReleaseFdWakeData() noexcept;
    void WakeSyncTask(std::unordered_map<CoTask*, EventVec>& syncTaskEvents) noexcept;

    void CacheEventsAndDoMask(CoTask* task, EventVec& eventVec) noexcept;
    int FetchCachedEventAndDoUnmask(CoTask* task, struct epoll_event* eventsVec) noexcept;
    int FetchCachedEventAndDoUnmask(EventVec& cachedEventsVec, struct epoll_event* eventsVec) noexcept;
    void CacheMaskFdAndEpollDel(int fd, CoTask* task) noexcept;
    int ClearMaskWakeDataCache(CoTask* task) noexcept;
    int ClearMaskWakeDataCacheWithFd(CoTask* task, int fd) noexcept;
    int ClearDelFdCache(int fd) noexcept;

    void WakeTask(CoTask* task);
    int CopyEventsToConsumer(EventVec& cachedEventsVec, struct epoll_event* eventsVec) noexcept;
    void CopyEventsInfoToConsumer(SyncData& taskInfo, EventVec& cachedEventsVec);

    void ThreadInit();
    virtual void Run() = 0;
    virtual bool IsIOPoller() = 0;
    virtual int PollerRegisterTimer(int qos, uint64_t timeout, void* data) = 0;
    virtual void PollerUnRegisterTimer(std::unordered_set<int>& timerHandlesToRemove) = 0;

    int epFd_;                        // epoll文件描述符
    struct PollerData wakeData_;
    mutable spin_mutex mapMutex_;     // 保护共享数据的互斥锁

    std::unordered_map<int, WakeDataList> wakeDataMap_; // fd到事件数据的映射
    std::unordered_map<int, int> delCntMap_; // 删除计数
    std::unordered_map<CoTask*, SyncData> waitTaskMap_; // 等待任务映射
    std::unordered_map<CoTask*, EventVec> cachedTaskEvents_; // 缓存的任务事件
    std::unordered_map<int, CoTask*> delFdCacheMap_; // 删除的fd缓存
    std::unordered_map<CoTask*, WakeDataList> maskWakeDataMap_; // 屏蔽的事件数据

    std::unique_ptr<std::thread> runner_ { nullptr }; // 轮询线程
    bool exitFlag_ { true }; // 线程退出标志
    bool teardown_ { false }; // 析构标志
    std::atomic<uint64_t> pollerCount_ { 0 }; // 轮询计数
    std::atomic_bool fdEmpty_ {true};
};
} // namespace ffrt
#endif