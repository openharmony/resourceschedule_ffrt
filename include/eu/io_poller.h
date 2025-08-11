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

#ifndef FFRT_POLLER_MANAGER_H
#define FFRT_POLLER_MANAGER_H

#include "eu/base_poller.h"
#ifndef _MSC_VER
#include <sys/epoll.h>
#include <sys/eventfd.h>
#endif
#include <thread>

namespace ffrt {
enum class PollerState {
    HANDLING, // worker执行事件回调（如果是同步回调函数执行有可能阻塞worker）
    POLLING, // worker处于epoll_wait睡眠（事件响应）
    EXITED, // worker没有事件时销毁线程（重新注册时触发创建线程）
};

struct TimeOutReport {
    TimeOutReport() {}
    std::atomic<uint64_t> cbStartTime = 0; // block info report
    uint64_t reportCount = 0;
};

class IOPoller : public BasePoller {
public:
    static IOPoller& Instance();
    virtual ~IOPoller() noexcept override;

    using BasePoller::WaitFdEvent;
    void WaitFdEvent(int fd) noexcept;

    void WakeTimeoutTask(CoTask* task) noexcept;
    void MonitTimeOut();

    bool IsIOPoller() override { return true; }
    int PollerRegisterTimer(int qos, uint64_t timeout, void* data) override;
    void PollerUnRegisterTimer(std::unordered_set<int>& timerHandlesToRemove) override;

private:
    void Run() override;
    int PollOnce(int timeout = -1) noexcept;

    struct TimeOutReport timeOutReport_;

    std::atomic_uint64_t syncFdCnt_ { 0 }; // record sync fd counts
};
}
#endif