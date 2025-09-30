/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "eu/base_poller.h"
#include <unordered_set>
#include "util/ffrt_facade.h"

namespace ffrt {
BasePoller::BasePoller() noexcept: epFd_ { ::epoll_create1(EPOLL_CLOEXEC) }
{
    if (epFd_ < 0) {
        FFRT_LOGE("epoll_create1 failed: errno=%d", errno);
    }
#ifdef OHOS_STANDARD_SYSTEM
    fdsan_exchange_owner_tag(epFd_, 0, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, static_cast<uint64_t>(epFd_)));
#endif
    wakeData_.mode = PollerType::WAKEUP;
    wakeData_.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wakeData_.fd < 0) {
        FFRT_LOGE("eventfd failed: errno=%d", errno);
    }
#ifdef OHOS_STANDARD_SYSTEM
    fdsan_exchange_owner_tag(wakeData_.fd, 0, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE,
        static_cast<uint64_t>(wakeData_.fd)));
#endif
    epoll_event ev { .events = EPOLLIN, .data = { .ptr = static_cast<void*>(&wakeData_) } };
    FFRT_COND_TERMINATE((epoll_ctl(epFd_, EPOLL_CTL_ADD, wakeData_.fd, &ev) < 0),
        "epoll_ctl add fd error: efd=%d, fd=%d, errorno=%d", epFd_, wakeData_.fd, errno);
}

BasePoller::~BasePoller() noexcept
{
#ifdef OHOS_STANDARD_SYSTEM
    fdsan_close_with_tag(wakeData_.fd, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE,
        static_cast<uint64_t>(wakeData_.fd)));
    fdsan_close_with_tag(epFd_, fdsan_create_owner_tag(FDSAN_OWNER_TYPE_FILE, static_cast<uint64_t>(epFd_)));
#else
    ::close(wakeData_.fd);
    ::close(epFd_);
#endif
}

void BasePoller::WakeTask(CoTask* task)
{
    std::unique_lock<std::mutex> lck(task->mutex_);
    if (task->GetBlockType() == BlockType::BLOCK_THREAD) {
        task->waitCond_.notify_one();
    } else {
        lck.unlock();
        CoRoutineFactory::CoWakeFunc(task, CoWakeType::NO_TIMEOUT_WAKE);
    }
}

int BasePoller::CopyEventsToConsumer(EventVec& cachedEventsVec, struct epoll_event* eventsVec) noexcept
{
    int nfds = static_cast<int>(cachedEventsVec.size());
    for (int i = 0; i < nfds; i++) {
        eventsVec[i].events = cachedEventsVec[i].events;
        eventsVec[i].data.fd = cachedEventsVec[i].data.fd;
    }
    return nfds;
}

void BasePoller::CopyEventsInfoToConsumer(SyncData& taskInfo, EventVec& cachedEventsVec)
{
    epoll_event* eventsPtr = (epoll_event*)taskInfo.eventsPtr;
    int* nfdsPtr = taskInfo.nfdsPtr;
    if (eventsPtr == nullptr || nfdsPtr == nullptr) {
        FFRT_LOGE("usr ptr is nullptr");
        return;
    }
    *nfdsPtr = CopyEventsToConsumer(cachedEventsVec, eventsPtr);
}

void BasePoller::ThreadInit()
{
    if (runner_ != nullptr && runner_->joinable()) {
        runner_->join();
    }
    runner_ = std::make_unique<std::thread>([this] { Run(); });
    exitFlag_ = false;
}

void BasePoller::WakeUp() noexcept
{
    uint64_t one = 1;
    (void)::write(wakeData_.fd, &one, sizeof one);
}

int BasePoller::AddFdEvent(int op, uint32_t events, int fd, void* data, ffrt_poller_cb cb) noexcept
{
    CoTask* task = IsCoTask(ExecuteCtx::Cur()->task) ? static_cast<CoTask*>(ExecuteCtx::Cur()->task) : nullptr;
    auto wakeData = std::make_unique<PollerData>(fd, data, cb, task);
    if (task) {
        task->pollerEnable = true;
    }
    void* ptr = static_cast<void*>(wakeData.get());
    if (ptr == nullptr || wakeData == nullptr) {
        FFRT_SYSEVENT_LOGE("Construct PollerData instance failed! or wakeData is nullptr");
        return -1;
    }
    wakeData->monitorEvents = events;

    epoll_event ev = { .events = events, .data = { .ptr = ptr } };
    std::lock_guard lock(mapMutex_);
    if (teardown_) {
        return -1;
    }

    if (IsIOPoller() && exitFlag_) {
        ThreadInit();
    }

    if (epoll_ctl(epFd_, op, fd, &ev) != 0) {
        FFRT_SYSEVENT_LOGE("epoll_ctl add fd error: efd=%d, fd=%d, errorno=%d", epFd_, fd, errno);
        return -1;
    }

    if (op == EPOLL_CTL_ADD) {
        wakeDataMap_[fd].emplace_back(std::move(wakeData));
        if (!IsIOPoller()) {
            fdEmpty_.store(false);
        }
    } else if (op == EPOLL_CTL_MOD) {
        auto iter = wakeDataMap_.find(fd);
        FFRT_COND_RETURN_ERROR(iter == wakeDataMap_.end(), -1, "fd %d does not exist in wakeDataMap", fd);
        if (iter->second.size() != 1) {
            FFRT_SYSEVENT_LOGE("epoll_ctl mod fd wakedata num invalid");
            return -1;
        }
        iter->second.pop_back();
        iter->second.emplace_back(std::move(wakeData));
    }
    return 0;
}

void BasePoller::CacheMaskFdAndEpollDel(int fd, CoTask* task) noexcept
{
    auto maskWakeData = maskWakeDataMap_.find(task);
    if (maskWakeData != maskWakeDataMap_.end()) {
        if (epoll_ctl(epFd_, EPOLL_CTL_DEL, fd, nullptr) != 0) {
            FFRT_SYSEVENT_LOGE("fd[%d] ffrt epoll ctl del fail errorno=%d", fd, errno);
        }
        delFdCacheMap_.emplace(fd, task);
    }
}

int BasePoller::ClearMaskWakeDataCache(CoTask* task) noexcept
{
    auto maskWakeDataIter = maskWakeDataMap_.find(task);
    if (maskWakeDataIter != maskWakeDataMap_.end()) {
        WakeDataList& wakeDataList = maskWakeDataIter->second;
        for (auto iter = wakeDataList.begin(); iter != wakeDataList.end(); ++iter) {
            PollerData* ptr = iter->get();
            delFdCacheMap_.erase(ptr->fd);
        }
        maskWakeDataMap_.erase(maskWakeDataIter);
    }
    return 0;
}

int BasePoller::ClearDelFdCache(int fd) noexcept
{
    auto fdDelCacheIter = delFdCacheMap_.find(fd);
    if (fdDelCacheIter != delFdCacheMap_.end()) {
        CoTask *task = fdDelCacheIter->second;
        ClearMaskWakeDataCacheWithFd(task, fd);
        delFdCacheMap_.erase(fdDelCacheIter);
    }
    return 0;
}

int BasePoller::ClearMaskWakeDataCacheWithFd(CoTask* task, int fd) noexcept
{
    auto maskWakeDataIter = maskWakeDataMap_.find(task);
    if (maskWakeDataIter != maskWakeDataMap_.end()) {
        WakeDataList& wakeDataList = maskWakeDataIter->second;
        auto pred = [fd](auto& value) { return value->fd == fd; };
        wakeDataList.remove_if(pred);
        if (wakeDataList.size() == 0) {
            maskWakeDataMap_.erase(maskWakeDataIter);
        }
    }
    return 0;
}

int BasePoller::DelFdEvent(int fd) noexcept
{
    std::unique_lock lock(mapMutex_);
    ClearDelFdCache(fd);
    auto wakeDataIter = wakeDataMap_.find(fd);
    if (wakeDataIter == wakeDataMap_.end() || wakeDataIter->second.size() == 0) {
        FFRT_SYSEVENT_LOGW("fd[%d] has not been added to epoll, ignore", fd);
        return -1;
    }
    auto delCntIter = delCntMap_.find(fd);
    if (delCntIter != delCntMap_.end()) {
        int diff = static_cast<int>(wakeDataIter->second.size()) - delCntIter->second;
        if (diff == 0) {
            FFRT_SYSEVENT_LOGW("fd:%d, addCnt:%d, delCnt:%d has not been added to epoll, ignore", fd,
                wakeDataIter->second.size(), delCntIter->second);
            return -1;
        }
    }

    if (epoll_ctl(epFd_, EPOLL_CTL_DEL, fd, nullptr) != 0) {
        FFRT_SYSEVENT_LOGE("epoll_ctl del fd error: efd=%d, fd=%d, errorno=%d", epFd_, fd, errno);
        return -1;
    }

    for (auto it = cachedTaskEvents_.begin(); it != cachedTaskEvents_.end();) {
        auto& events = it->second;
        events.erase(std::remove_if(events.begin(), events.end(),
            [fd](const epoll_event& event) {
                return event.data.fd == fd;
            }), events.end());
        if (events.empty()) {
            it = cachedTaskEvents_.erase(it);
        } else {
            ++it;
        }
    }

    delCntMap_[fd]++;
    WakeUp();
    return 0;
}

// mode ASYNC_IO
int BasePoller::WaitFdEvent(struct epoll_event* eventsVec, int maxevents, int timeout) noexcept
{
    FFRT_COND_DO_ERR((eventsVec == nullptr), return -1, "eventsVec cannot be null");

    CoTask* task = IsCoTask(ExecuteCtx::Cur()->task) ? static_cast<CoTask*>(ExecuteCtx::Cur()->task) : nullptr;
    if (!task) {
        FFRT_SYSEVENT_LOGE("nonworker shall not call this fun.");
        return -1;
    }

    FFRT_COND_DO_ERR((maxevents < EPOLL_EVENT_SIZE), return -1, "maxEvents:%d cannot be less than 1024", maxevents);

    int nfds = 0;
    std::unique_lock<std::mutex> lck(task->mutex_);
    if (task->Block() == BlockType::BLOCK_THREAD) {
        mapMutex_.lock();
        int cachedNfds = FetchCachedEventAndDoUnmask(task, eventsVec);
        if (cachedNfds > 0) {
            mapMutex_.unlock();
            FFRT_LOGD("task[%s] id[%d] has [%d] cached events, return directly",
                task->GetLabel().c_str(), task->gid, cachedNfds);
            task->Wake();
            return cachedNfds;
        }

        if (waitTaskMap_.find(task) != waitTaskMap_.end()) {
            FFRT_SYSEVENT_LOGE("task has waited before");
            mapMutex_.unlock();
            task->Wake();
            return 0;
        }
        auto currTime = std::chrono::steady_clock::now();
        waitTaskMap_[task] = {static_cast<void*>(eventsVec), maxevents, &nfds, currTime};
        if (timeout > -1) {
            FFRT_LOGD("poller meet timeout={%d}", timeout);
            waitTaskMap_[task].timerHandle = PollerRegisterTimer(task->qos_, timeout,
                reinterpret_cast<void*>(task));
        }
        mapMutex_.unlock();
        task->waitCond_.wait(lck);
        FFRT_LOGD("task[%s] id[%d] has [%d] events", task->GetLabel().c_str(), task->gid, nfds);
        task->Wake();
        return nfds;
    }
    lck.unlock();

    CoWait([&](CoTask* task)->bool {
        mapMutex_.lock();
        int cachedNfds = FetchCachedEventAndDoUnmask(task, eventsVec);
        if (cachedNfds > 0) {
            mapMutex_.unlock();
            FFRT_LOGD("task[%s] id[%d] has [%d] cached events, return directly",
                task->GetLabel().c_str(), task->gid, cachedNfds);
            nfds = cachedNfds;
            return false;
        }

        if (waitTaskMap_.find(task) != waitTaskMap_.end()) {
            FFRT_SYSEVENT_LOGE("task has waited before");
            mapMutex_.unlock();
            return false;
        }
        auto currTime = std::chrono::steady_clock::now();
        waitTaskMap_[task] = {static_cast<void*>(eventsVec), maxevents, &nfds, currTime};
        if (timeout > -1) {
            FFRT_LOGD("poller meet timeout={%d}", timeout);
            waitTaskMap_[task].timerHandle = PollerRegisterTimer(task->qos_, timeout,
                reinterpret_cast<void*>(task));
        }
        mapMutex_.unlock();
        // The ownership of the task belongs to waitTaskMap_, and the task cannot be accessed any more.
        return true;
    });
    FFRT_LOGD("task[%s] id[%d] has [%d] events", task->GetLabel().c_str(), task->gid, nfds);
    return nfds;
}

void BasePoller::WakeSyncTask(std::unordered_map<CoTask*, EventVec>& syncTaskEvents) noexcept
{
    if (syncTaskEvents.empty()) {
        return;
    }

    std::unordered_set<int> timerHandlesToRemove;
    std::unordered_set<CoTask*> tasksToWake;
    mapMutex_.lock();
    for (auto& taskEventPair : syncTaskEvents) {
        CoTask* currTask = taskEventPair.first;
        auto iter = waitTaskMap_.find(currTask);
        if (iter == waitTaskMap_.end()) {  // task not in wait map
            CacheEventsAndDoMask(currTask, taskEventPair.second);
            continue;
        }
        CopyEventsInfoToConsumer(iter->second, taskEventPair.second);
        // remove timer, wake task, erase from wait map
        auto timerHandle = iter->second.timerHandle;
        if (timerHandle > -1) {
            timerHandlesToRemove.insert(timerHandle);
        }
        tasksToWake.insert(currTask);
        waitTaskMap_.erase(iter);
    }
    mapMutex_.unlock();
    PollerUnRegisterTimer(timerHandlesToRemove);

    for (auto task : tasksToWake) {
        WakeTask(task);
    }
}

int BasePoller::FetchCachedEventAndDoUnmask(EventVec& cachedEventsVec, struct epoll_event* eventsVec) noexcept
{
    std::unordered_map<int, int> seenFd;
    int fdCnt = 0;
    for (size_t i = 0; i < cachedEventsVec.size(); i++) {
        auto eventInfo = cachedEventsVec[i];
        int currFd = eventInfo.data.fd;
        // check if seen
        auto iter = seenFd.find(currFd);
        if (iter == seenFd.end()) {
            // if not seen, copy cached events and record idx
            eventsVec[fdCnt].data.fd = currFd;
            eventsVec[fdCnt].events = eventInfo.events;
            seenFd[currFd] = fdCnt;
            fdCnt++;
        } else {
            // if seen, update event to newest
            eventsVec[iter->second].events |= eventInfo.events;
            FFRT_LOGD("fd[%d] has mutilple cached events", currFd);
            continue;
        }

        // Unmask to origin events
        auto wakeDataIter = wakeDataMap_.find(currFd);
        if (wakeDataIter == wakeDataMap_.end() || wakeDataIter->second.size() == 0) {
            FFRT_LOGD("fd[%d] may be deleted", currFd);
            continue;
        }

        auto& wakeData = wakeDataIter->second.back();
        epoll_event ev = { .events = wakeData->monitorEvents, .data = { .ptr = static_cast<void*>(wakeData.get()) } };
        auto fdDelCacheIter = delFdCacheMap_.find(currFd);
        if (fdDelCacheIter != delFdCacheMap_.end()) {
            ClearDelFdCache(currFd);
            if (epoll_ctl(epFd_, EPOLL_CTL_ADD, currFd, &ev) != 0) {
                FFRT_SYSEVENT_LOGE("fd[%d] epoll ctl add fail, errorno=%d", currFd, errno);
                continue;
            }
        } else {
            if (epoll_ctl(epFd_, EPOLL_CTL_MOD, currFd, &ev) != 0) {
                FFRT_SYSEVENT_LOGE("fd[%d] epoll ctl mod fail, errorno=%d", currFd, errno);
                continue;
            }
        }
    }
    return fdCnt;
}

int BasePoller::FetchCachedEventAndDoUnmask(CoTask* task, struct epoll_event* eventsVec) noexcept
{
    // should used in lock
    auto syncTaskIter = cachedTaskEvents_.find(task);
    if (syncTaskIter == cachedTaskEvents_.end() || syncTaskIter->second.size() == 0) {
        return 0;
    }

    int nfds = FetchCachedEventAndDoUnmask(syncTaskIter->second, eventsVec);
    cachedTaskEvents_.erase(syncTaskIter);
    ClearMaskWakeDataCache(task);
    return nfds;
}

void BasePoller::CacheEventsAndDoMask(CoTask* task, EventVec& eventVec) noexcept
{
    auto& syncTaskEvents = cachedTaskEvents_[task];
    for (size_t i = 0; i < eventVec.size(); i++) {
        int currFd = eventVec[i].data.fd;

        auto wakeDataIter = wakeDataMap_.find(currFd);
        if (wakeDataIter == wakeDataMap_.end() ||
            wakeDataIter->second.size() == 0 ||
            wakeDataIter->second.back()->task != task) {
            FFRT_LOGD("fd[%d] may be deleted", currFd);
            continue;
        }

        auto delIter = delCntMap_.find(currFd);
        if (delIter != delCntMap_.end() && wakeDataIter->second.size() == static_cast<size_t>(delIter->second)) {
            FFRT_LOGD("fd[%d] may be deleted", currFd);
            continue;
        }

        struct epoll_event maskEv;
        maskEv.events = 0;
        auto& wakeData = wakeDataIter->second.back();
        std::unique_ptr<struct PollerData> maskWakeData = std::make_unique<PollerData>(currFd,
            wakeData->data, wakeData->cb, wakeData->task);
        void* ptr = static_cast<void*>(maskWakeData.get());
        if (ptr == nullptr || maskWakeData == nullptr) {
            FFRT_SYSEVENT_LOGE("CacheEventsAndDoMask Construct PollerData instance failed! or wakeData is nullptr");
            continue;
        }
        maskWakeData->monitorEvents = 0;
        maskWakeDataMap_[task].emplace_back(std::move(maskWakeData));

        maskEv.data = {.ptr = ptr};
        if (epoll_ctl(epFd_, EPOLL_CTL_MOD, currFd, &maskEv) != 0 && errno != ENOENT) {
            // ENOENT indicate fd is not in epfd, may be deleted
            FFRT_SYSEVENT_LOGW("epoll_ctl mod fd error: efd=%d, fd=%d, errorno=%d", epFd_, currFd, errno);
        }
        FFRT_LOGD("fd[%d] event has no consumer, so cache it", currFd);
        syncTaskEvents.push_back(eventVec[i]);
    }
}

void BasePoller::ReleaseFdWakeData() noexcept
{
    std::unique_lock lock(mapMutex_);
    for (auto delIter = delCntMap_.begin(); delIter != delCntMap_.end();) {
        int delFd = delIter->first;
        unsigned int delCnt = static_cast<unsigned int>(delIter->second);
        auto& wakeDataList = wakeDataMap_[delFd];
        unsigned int diff = wakeDataList.size() - delCnt;
        if (diff == 0) {
            wakeDataMap_.erase(delFd);
            delCntMap_.erase(delIter++);
            continue;
        } else if (diff == 1) {
            for (unsigned int i = 0; i < delCnt - 1; i++) {
                wakeDataList.pop_front();
            }
            delCntMap_[delFd] = 1;
        } else {
            FFRT_SYSEVENT_LOGE("fd=%d count unexpected, added num=%d, del num=%d", delFd, wakeDataList.size(), delCnt);
        }
        delIter++;
    }
    if (!IsIOPoller()) {
        fdEmpty_.store(wakeDataMap_.empty());
    }
}
}