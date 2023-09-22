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

#ifndef HICORO_IOPOLLER_H
#define HICORO_IOPOLLER_H

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <thread>
#include <vector>
#include <map>
#include <atomic>
#include "ffrt_inner.h"
#include "internal_inc/non_copyable.h"

namespace ffrt {
struct WakeData {
    int fd;
    void* data;
};

struct IOPoller: private NonCopyable {
    IOPoller() noexcept;

    virtual ~IOPoller() noexcept;

    void WakeUp() noexcept;
    bool CasStrong(std::atomic<int> &a, int cmp, int exc);
    void WaitFdEvent(int fd) noexcept;
    void PollOnce(int timeout = -1) noexcept;

private:
    int m_epFd;
    struct WakeData m_wakeData;
    std::vector<epoll_event> m_events;
};

IOPoller& GetIOPoller() noexcept;
}

static inline void ffrt_wait_fd(int fd)
{
    ffrt::GetIOPoller().WaitFdEvent(fd);
}
#endif
