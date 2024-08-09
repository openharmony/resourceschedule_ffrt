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
namespace {
    const int FFRT_DELAY_WORKER_TIMEOUT_SECONDS = 180;
}
namespace ffrt {
void DelayedWorker::ThreadInit()
{
    if (delayWorker != nullptr && delayWorker->joinable()) {
        delayWorker->join();
    }
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
                exited_ = true;
                break;
            }
            int result = HandleWork();
            if (toExit) {
                exited_ = true;
                break;
            }
            if (result == 0) {
                auto time_out = map.begin()->first;
                cv.wait_until(lk, time_out);
            } else if (result == 1) {
                if (++noTaskDelayCount_ > 1) {
                    exited_ = true;
                    break;
                }
                cv.wait_until(lk, std::chrono::steady_clock::now() +
                    std::chrono::seconds(FFRT_DELAY_WORKER_TIMEOUT_SECONDS));
            }
        }
    });
}

DelayedWorker::DelayedWorker()
{
    ThreadInit();
}

DelayedWorker::~DelayedWorker()
{
    lock.lock();
    toExit = true;
    lock.unlock();
    cv.notify_one();
    if (delayWorker != nullptr && delayWorker->joinable()) {
        delayWorker->join();
    }
}

DelayedWorker& DelayedWorker::GetInstance()
{
    static DelayedWorker instance;
    return instance;
}

int DelayedWorker::HandleWork()
{
    if (!map.empty()) {
        noTaskDelayCount_ = 0;
        do {
            TimePoint now = std::chrono::steady_clock::now();
            auto cur = map.begin();
            if (!toExit && cur != nullptr && cur->first <= now) {
                DelayedWork w = cur->second;
                map.erase(cur);
                lock.unlock();
                (*w.cb)(w.we);
                lock.lock();
                FFRT_COND_DO_ERR(toExit, return -1, "HandleWork exit, map size:%d", map.size());
            } else {
                return 0;
            }
        } while (!map.empty());
    }
    return 1;
}

// There is no requirement that to be less than now
bool DelayedWorker::dispatch(const TimePoint& to, WaitEntry* we, const std::function<void(WaitEntry*)>& wakeup)
{
    bool w = false;
    lock.lock();
    if (toExit) {
        lock.unlock();
        FFRT_LOGE("DelayedWorker destroy, dispatch failed\n");
        return false;
    }

    TimePoint now = std::chrono::steady_clock::now();
    if (to <= now) {
        lock.unlock();
        return false;
    }

    if (exited_) {
        ThreadInit();
        exited_ = false;
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

bool DelayedWorker::remove(const TimePoint& to, WaitEntry* we)
{
    std::lock_guard<decltype(lock)> l(lock);

    auto range = map.equal_range(to);
    for (auto it = range.first; it != range.second; ++it) {
        if (it->second.we == we) {
            map.erase(it);
            return true;
        }
    }

    return false;
}
} // namespace ffrt