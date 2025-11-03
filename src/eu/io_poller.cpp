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
#include "eu/io_poller.h"
#include <securec.h>
#include <sys/prctl.h>
#include "eu/blockaware.h"
#include "eu/execute_unit.h"
#include "sched/execute_ctx.h"
#include "tm/scpu_task.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/ffrt_facade.h"
#include "util/time_format.h"
#include "util/name_manager.h"
#include "sync/timer_manager.h"
#ifdef FFRT_OH_TRACE_ENABLE
#include "backtrace_local.h"
#endif

namespace {
const std::vector<uint64_t> TIMEOUT_RECORD_CYCLE_LIST = { 1, 3, 5, 10, 30, 60, 10 * 60, 30 * 60 };
}

namespace ffrt {
namespace {
static void TimeoutProc(void* task)
{
    IOPoller& ins = IOPoller::Instance();
    ins.WakeTimeoutTask(reinterpret_cast<CoTask*>(task));
}
}

int IOPoller::PollerRegisterTimer(int qos, uint64_t timeout, void *data)
{
    return FFRTFacade::GetTMInstance().RegisterTimer(qos, timeout, data, TimeoutProc);
}

void IOPoller::PollerUnRegisterTimer(std::unordered_set<int>& timerHandlesToRemove)
{
    for (auto timerHandle : timerHandlesToRemove) {
        FFRTFacade::GetTMInstance().UnregisterTimer(timerHandle);
    }
}

IOPoller& IOPoller::Instance()
{
    static IOPoller ins;
    return ins;
}

IOPoller::~IOPoller() noexcept
{
    {
        std::lock_guard lock(mapMutex_);
        teardown_ = true;
        WakeUp();
    }
    if (runner_ != nullptr && runner_->joinable()) {
        runner_->join();
    }
}

void IOPoller::Run()
{
    struct sched_param param;
    param.sched_priority = 1;
    ExecuteCtx::Cur()->threadType_ = ffrt::ThreadType::IO_POLLER;
    int ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    if (ret != 0) {
        FFRT_LOGW("[%d] set priority warn ret[%d] eno[%d]\n", pthread_self(), ret, errno);
    }
    prctl(PR_SET_NAME, IO_POLLER_NAME);
    ioPid_ = syscall(SYS_gettid);
    while (1) {
        ret = PollOnce(30000);
        std::lock_guard lock(mapMutex_);
        if (teardown_) {
            // teardown
            exitFlag_ = true;
            return;
        }
        if (ret == 0 && wakeDataMap_.empty() && syncFdCnt_.load() == 0) {
            // timeout 30s and no fd added
            exitFlag_ = true;
            return;
        }
    }
}

int IOPoller::PollOnce(int timeout) noexcept
{
    pollerCount_++;
    std::array<epoll_event, EPOLL_EVENT_SIZE> waitedEvents;
    int nfds = epoll_wait(epFd_, waitedEvents.data(), waitedEvents.size(), timeout);
    if (nfds < 0) {
        if (errno != EINTR) {
            FFRT_SYSEVENT_LOGE("epoll_wait error, errorno= %d.", errno);
        }
        return -1;
    }
    if (nfds == 0) {
        return 0;
    }

    std::unordered_map<CoTask*, EventVec> syncTaskEvents;
    for (unsigned int i = 0; i < static_cast<unsigned int>(nfds); ++i) {
        struct PollerData *data = reinterpret_cast<struct PollerData *>(waitedEvents[i].data.ptr);
        if (data->mode == PollerType::WAKEUP) {
            // self wakeup
            uint64_t one = 1;
            (void)::read(wakeData_.fd, &one, sizeof one);
            continue;
        }

        if (data->mode == PollerType::SYNC_IO) {
            // sync io wait fd, del fd when waked up
            if (epoll_ctl(epFd_, EPOLL_CTL_DEL, data->fd, nullptr) != 0) {
                FFRT_SYSEVENT_LOGE("epoll_ctl del fd error: fd=%d, errorno=%d", data->fd, errno);
                continue;
            }
            syncFdCnt_--;
            WakeTask(data->task);
            continue;
        }

        if (data->mode == PollerType::ASYNC_CB) {
            // async io callback
            timeOutReport_.cbStartTime.store(TimeStamp(), std::memory_order_relaxed);
            timeOutReport_.reportCount.store(0, std::memory_order_relaxed);
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (data->traceId.valid == HITRACE_ID_VALID) {
                TraceChainAdapter::Instance().HiTraceChainRestoreId(&data->traceId);
            }
#endif
            FFRT_TRACE_BEGIN("IOCB");
            data->cb(data->data, waitedEvents[i].events);
            FFRT_TRACE_END();
            timeOutReport_.cbStartTime.store(0, std::memory_order_relaxed);
#ifdef FFRT_ENABLE_HITRACE_CHAIN
            if (data->traceId.valid == HITRACE_ID_VALID) {
                TraceChainAdapter::Instance().HiTraceChainClearId();
            }
#endif
            continue;
        }

        if (data->mode == PollerType::ASYNC_IO) {
            // async io task wait fd
            epoll_event ev = { .events = waitedEvents[i].events, .data = {.fd = data->fd} };
            syncTaskEvents[data->task].push_back(ev);
            if ((waitedEvents[i].events & (EPOLLHUP | EPOLLERR)) != 0) {
                std::lock_guard lock(mapMutex_);
                CacheMaskFdAndEpollDel(data->fd, data->task);
            }
        }
    }

    WakeSyncTask(syncTaskEvents);
    ReleaseFdWakeData();
    return 1;
}

void IOPoller::WakeTimeoutTask(CoTask* task) noexcept
{
    mapMutex_.lock();
    auto iter = waitTaskMap_.find(task);
    if (iter != waitTaskMap_.end()) {
        // wake task, erase from wait map
        waitTaskMap_.erase(iter);
        mapMutex_.unlock();
        WakeTask(task);
    } else {
        // already erase from wait map
        mapMutex_.unlock();
    }
}

// mode SYNC_IO
void IOPoller::WaitFdEvent(int fd) noexcept
{
    CoTask* task = IsCoTask(ExecuteCtx::Cur()->task) ? static_cast<CoTask*>(ExecuteCtx::Cur()->task) : nullptr;
    if (!task) {
        FFRT_LOGI("nonworker shall not call this fun.");
        return;
    }

    struct PollerData data(fd, task);
    epoll_event ev = { .events = EPOLLIN, .data = {.ptr = static_cast<void*>(&data)} };
    {
        std::lock_guard lock(mapMutex_);
        if (teardown_) {
            return;
        }

        if (exitFlag_) {
            ThreadInit();
            exitFlag_ = false;
        }

        syncFdCnt_++;
    }

    FFRT_BLOCK_TRACER(task->gid, fd);
    if (task->Block() == BlockType::BLOCK_THREAD) {
        std::unique_lock<std::mutex> lck(task->mutex_);
        if (epoll_ctl(epFd_, EPOLL_CTL_ADD, fd, &ev) == 0) {
            task->waitCond_.wait(lck);
        }
        task->Wake();
        syncFdCnt_--;
        return;
    }

    CoWait([&](CoTask* task)->bool {
        (void)task;
        if (epoll_ctl(epFd_, EPOLL_CTL_ADD, fd, &ev) == 0) {
            return true;
        }
        // The ownership of the task belongs to epoll, and the task cannot be accessed any more.
        FFRT_LOGI("epoll_ctl add err:efd:=%d, fd=%d errorno = %d", epFd_, fd, errno);
        syncFdCnt_--;
        return false;
    });
}

void IOPoller::MonitTimeOut()
{
    if (teardown_) {
        return;
    }

    if (timeOutReport_.cbStartTime.load(std::memory_order_relaxed) == 0) {
        return;
    }
    uint64_t now = TimeStamp();
    static const uint64_t freq = [] {
        uint64_t f = Arm64CntFrq();
        return (f == 1) ? 1000000 : f;
    } ();
    uint64_t diff = (now - timeOutReport_.cbStartTime.load(std::memory_order_relaxed)) / freq;
    uint64_t reportTime = TIMEOUT_RECORD_CYCLE_LIST[timeOutReport_.reportCount];
    if (timeOutReport_.reportCount.load(std::memory_order_relaxed) < TIMEOUT_RECORD_CYCLE_LIST.size() &&
        diff >= TIMEOUT_RECORD_CYCLE_LIST[timeOutReport_.reportCount.load(std::memory_order_relaxed)]) {
#ifdef FFRT_OH_TRACE_ENABLE
        std::string dumpInfo;
        if (OHOS::HiviewDFX::GetBacktraceStringByTid(dumpInfo, ioPid_, 0, false)) {
            FFRT_LOGW("IO_Poller Backtrace Info[%lus]:\n%s", reportTime, dumpInfo.c_str());
        }
#endif
        timeOutReport_.reportCount.fetch_add(1, std::memory_order_relaxed);
    }
}
}