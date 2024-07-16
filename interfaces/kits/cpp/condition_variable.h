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

/**
 * @file condition_variable.h
 *
 * @brief Declares the condition variable interfaces in C++.
 *
 * @since 10
 * @version 1.0
 */
#ifndef FFRT_API_CPP_CONDITION_VARIABLE_H
#define FFRT_API_CPP_CONDITION_VARIABLE_H
#include <chrono>
#include <mutex>
#include "mutex.h"
#include "c/condition_variable.h"

namespace ffrt {
enum class cv_status { no_timeout, timeout };

class condition_variable : public ffrt_cond_t {
public:
    condition_variable()
    {
        ffrt_cond_init(this, nullptr);
    }

    ~condition_variable() noexcept
    {
        ffrt_cond_destroy(this);
    }

    condition_variable(const condition_variable&) = delete;
    condition_variable& operator=(const condition_variable&) = delete;

    template <typename Clock, typename Duration, typename Pred>
    bool wait_until(
        std::unique_lock<mutex>& lk, const std::chrono::time_point<Clock, Duration>& tp, Pred&& pred) noexcept
    {
        while (!pred()) {
            if (wait_until(lk, tp) == cv_status::timeout) {
                return pred();
            }
        }
        return true;
    }

    template <typename Clock, typename Duration>
    cv_status wait_until(std::unique_lock<mutex>& lk, const std::chrono::time_point<Clock, Duration>& tp) noexcept
    {
        return _wait_for(lk, tp - Clock::now());
    }

    template <typename Rep, typename Period>
    cv_status wait_for(std::unique_lock<mutex>& lk, const std::chrono::duration<Rep, Period>& sleep_time) noexcept
    {
        return _wait_for(lk, sleep_time);
    }

    template <typename Rep, typename Period, typename Pred>
    bool wait_for(
        std::unique_lock<mutex>& lk, const std::chrono::duration<Rep, Period>& sleepTime, Pred&& pred) noexcept
    {
        return wait_until(lk, std::chrono::steady_clock::now() + sleepTime, std::forward<Pred>(pred));
    }

    template <typename Pred>
    void wait(std::unique_lock<mutex>& lk, Pred&& pred)
    {
        while (!pred()) {
            wait(lk);
        }
    }

    void wait(std::unique_lock<mutex>& lk)
    {
        ffrt_cond_wait(this, lk.mutex());
    }

    void notify_one() noexcept
    {
        ffrt_cond_signal(this);
    }

    void notify_all() noexcept
    {
        ffrt_cond_broadcast(this);
    }

private:
    template <typename Rep, typename Period>
    cv_status _wait_for(std::unique_lock<mutex>& lk, const std::chrono::duration<Rep, Period>& dur) noexcept
    {
        timespec ts;
        std::chrono::nanoseconds ns = std::chrono::steady_clock::now().time_since_epoch();
        ns += std::chrono::duration_cast<std::chrono::nanoseconds>(dur);
        ts.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(ns).count();
        ns -= std::chrono::seconds(ts.tv_sec);
        ts.tv_nsec = static_cast<long>(ns.count());

        auto ret = ffrt_cond_timedwait(this, lk.mutex(), &ts);
        if (ret == ffrt_success) {
            return cv_status::no_timeout;
        }
        return cv_status::timeout;
    }
};
} // namespace ffrt
#endif
