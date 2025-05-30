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

#include "io_poller.h"
#include "sched/execute_ctx.h"
#include "eu/co_routine.h"
#include "dfx/log/ffrt_log_api.h"
#include "ffrt_trace.h"
#include "internal_inc/assert.h"
#include "internal_inc/types.h"
#include "tm/scpu_task.h"
#include "util/ffrt_facade.h"
#include "util/name_manager.h"

namespace ffrt {
constexpr unsigned int DEFAULT_CPUINDEX_LIMIT = 7;
struct IOPollerInstance: public IOPoller {
    IOPollerInstance() noexcept: m_runner([&] { RunForever(); })
    {
        DependenceManager::Instance();
        pthread_setname_np(m_runner.native_handle(), IO_POLLER_NAME);
    }

    void RunForever() noexcept
    {
        struct sched_param param;
        param.sched_priority = 1;
        int ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
        if (ret != 0) {
            FFRT_LOGW("[%d] set priority warn ret[%d] eno[%d]\n", pthread_self(), ret, errno);
        }
        while (!m_exitFlag.load(std::memory_order_relaxed)) {
            IOPoller::PollOnce(-1);
        }
    }

    ~IOPollerInstance() noexcept override
    {
        Stop();
        m_runner.join();
    }
private:
    void Stop() noexcept
    {
        m_exitFlag.store(true, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acq_rel);
        IOPoller::WakeUp();
    }

private:
    // The order of these two lines matters!
    // The flag must be initialized before
    // the thread, otherwise a data-race
    // can occur when the flag is read
    // in RunForever, while being
    // initialized.
    std::atomic<bool> m_exitFlag { false };
    std::thread m_runner;
};

IOPoller& GetIOPoller() noexcept
{
    static IOPollerInstance inst;
    return inst;
}

IOPoller::IOPoller() noexcept: m_epFd { ::epoll_create1(EPOLL_CLOEXEC) },
    m_events(32)
{
    if (m_epFd < 0) {
        FFRT_LOGE("epoll_create1 failed: errno=%d", errno);
    }
    {
        m_wakeData.data = nullptr;
        m_wakeData.fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (m_wakeData.fd < 0) {
            FFRT_LOGE("eventfd failed: errno=%d", errno);
        }
        epoll_event ev{ .events = EPOLLIN, .data = { .ptr = static_cast<void*>(&m_wakeData) } };
        FFRT_COND_TERMINATE((epoll_ctl(m_epFd, EPOLL_CTL_ADD, m_wakeData.fd, &ev) < 0), "io epoll_ctl add fd error");
    }
}

IOPoller::~IOPoller() noexcept
{
    ::close(m_wakeData.fd);
    ::close(m_epFd);
}

bool IOPoller::CasStrong(std::atomic<int>& a, int cmp, int exc)
{
    return a.compare_exchange_strong(cmp, exc);
}

void IOPoller::WakeUp() noexcept
{
    uint64_t one = 1;
    ssize_t n = ::write(m_wakeData.fd, &one, sizeof one);
    FFRT_ASSERT(n == sizeof one);
}

void IOPoller::WaitFdEvent(int fd) noexcept
{
    CoTask* task = IsCoTask(ExecuteCtx::Cur()->task) ? static_cast<CoTask*>(ExecuteCtx::Cur()->task) : nullptr;
    if (!task) {
        FFRT_SYSEVENT_LOGI("nonworker shall not call this fun.");
        return;
    }

    struct WakeData data = {.fd = fd, .data = static_cast<void *>(task)};

    epoll_event ev = { .events = EPOLLIN, .data = {.ptr = static_cast<void*>(&data)} };
    FFRT_BLOCK_TRACER(task->gid, fd);
    if (ThreadWaitMode(task)) {
        std::unique_lock<std::mutex> lck(task->mutex_);
        if (epoll_ctl(m_epFd, EPOLL_CTL_ADD, fd, &ev) == 0) {
            if (FFRT_UNLIKELY(LegacyMode(task))) {
                task->blockType = BlockType::BLOCK_THREAD;
            }
            task->waitCond_.wait(lck);
        }
        return;
    }

    CoWait([&](CoTask *task)->bool {
        (void)task;
        if (epoll_ctl(m_epFd, EPOLL_CTL_ADD, fd, &ev) == 0) {
            return true;
        }
        // The ownership of the task belongs to epoll, and the task cannot be accessed any more.
        FFRT_LOGI("epoll_ctl add err:efd:=%d, fd=%d errorno = %d", m_epFd, fd, errno);
        return false;
    });
}

void IOPoller::PollOnce(int timeout) noexcept
{
    int ndfs = epoll_wait(m_epFd, m_events.data(), m_events.size(), timeout);
    if (ndfs <= 0) {
        if (errno != EINTR) {
            FFRT_SYSEVENT_LOGE("epoll_wait error: efd = %d, errorno= %d", m_epFd, errno);
        }
        return;
    }

    for (unsigned int i = 0; i < static_cast<unsigned int>(ndfs); ++i) {
        struct WakeData *data = reinterpret_cast<struct WakeData *>(m_events[i].data.ptr);

        if (data->fd == m_wakeData.fd) {
            uint64_t one = 1;
            ssize_t n = ::read(m_wakeData.fd, &one, sizeof one);
            FFRT_ASSERT(n == sizeof one);
            continue;
        }

        if (epoll_ctl(m_epFd, EPOLL_CTL_DEL, data->fd, nullptr) != 0) {
            FFRT_SYSEVENT_LOGI("epoll_ctl fd = %d errorno = %d", data->fd, errno);
            continue;
        }

        auto task = reinterpret_cast<CoTask *>(data->data);
        if (ThreadNotifyMode(task)) {
            std::unique_lock<std::mutex> lck(task->mutex_);
            if (BlockThread(task)) {
                task->blockType = BlockType::BLOCK_COROUTINE;
            }
            task->waitCond_.notify_one();
        } else {
            CoRoutineFactory::CoWakeFunc(task, CoWakeType::NO_TIMEOUT_WAKE);
        }
    }
}
}

void ffrt_wait_fd(int fd)
{
    ffrt::FFRTFacade::GetIoPPInstance().WaitFdEvent(fd);
}
