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

#include "delayed_worker.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <thread>
#include "dfx/log/ffrt_log_api.h"
#include "util/name_manager.h"
namespace ffrt {
DelayedWorker::DelayedWorker()
{
    delayWorker = std::make_unique<std::thread>([this]() {
    struct sched_param param;
    param.sched_priority = 1;
    int ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    if (ret != 0) {
        FFRT_LOGE("[%d] set priority failed ret[%d] errno[%d]\n", pthread_self(), ret, errno);
    }
        prctl(PR_SET_NAME, DELAYED_WORKER_NAME);
        for (;;) {
            std::unique_lock lk(lock);
            if (toExit) {
                break;
            }
            int ret = HandleWork();
            if (toExit) {
                break;
            }
            if (ret == 0) {
                cv.wait_until(lk, map.begin()->first);
            } else if (ret == 1) {
                cv.wait_until(lk, std::chrono::steady_clock::now() + std::chrono::hours(1));
            }
        }
    });
}

DelayedWorker::~DelayedWorker()
{
    lock.lock();
    toExit = true;
    lock.unlock();

    cv.notify_one();
    delayWorker->join();
}

int DelayedWorker::HandleWork()
{
    while (!map.empty()) {
        time_point_t now = std::chrono::steady_clock::now();
        auto cur = map.begin();
        if (cur->first <= now) {
            DelayedWork w = cur->second;
            map.erase(cur);
            lock.unlock();
            (*w.cb)(w.we);
            lock.lock();
            if (toExit) {
                return -1;
            }
        } else {
            return 0;
        }
    }
    return 1;
}

bool DelayedWorker::dispatch(const time_point_t& to, WaitEntry* we, const std::function<void(WaitEntry*)>& wakeup)
{
    bool w = false;
    lock.lock();

    if (toExit) {
        lock.unlock();
        return false;
    }

    time_point_t now = std::chrono::steady_clock::now();
    if (to <= now) {
        lock.unlock();
        return false;
    }

    if (map.empty() || to < map.begin()->first) {
        w = true;
    }
    map.emplace(to, DelayedWork {we, &wakeup});
    lock.unlock();
    if (w) {
        cv.notify_one();
    }

    return true;
}
} // namespace ffrt