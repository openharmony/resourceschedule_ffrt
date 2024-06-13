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

#ifndef _DELAYED_WORKER_H_
#define _DELAYED_WORKER_H_

#include <map>
#include <functional>
#include "cpp/sleep.h"
#include "sched/execute_ctx.h"
namespace ffrt {
using time_point_t = std::chrono::steady_clock::time_point;

struct DelayedWork {
    WaitEntry* we;
    const std::function<void(WaitEntry*)>* cb;
};

class DelayedWorker {
    std::multimap<time_point_t, DelayedWork> map;
    std::mutex lock;
    std::atomic_bool toExit = false;
    std::condition_variable cv;
    std::unique_ptr<std::thread> delayWorker = nullptr;

    int HandleWork(void);

public:
    DelayedWorker(DelayedWorker const&) = delete;
    void operator=(DelayedWorker const&) = delete;

    DelayedWorker();

    ~DelayedWorker();

    bool dispatch(const time_point_t& to, WaitEntry* we, const std::function<void(WaitEntry*)>& wakeup);
};
} // namespace ffrt
#endif
