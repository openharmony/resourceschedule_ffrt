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
#ifndef FFRT_API_CPP_THREAD_H
#define FFRT_API_CPP_THREAD_H
#include <memory>
#include "task.h"

namespace ffrt {
class thread {
public:
    thread() noexcept
    {
    }

    template <typename Fn, typename... Args>
    explicit thread(std::string& name, enum qos qos, Fn&& fn, Args&&... args)
    {
        is_joinable = std::make_unique<bool>(true);
        ffrt::submit(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...), {}, {is_joinable.get()},
            ffrt::task_attr().name(name.c_str()).qos(qos));
    }

    template <typename Fn, typename... Args>
    explicit thread(enum qos qos, Fn&& fn, Args&&... args)
    {
        is_joinable = std::make_unique<bool>(true);
        ffrt::submit(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...), {}, {is_joinable.get()},
            ffrt::task_attr().qos(qos));
    }

    template <typename Fn, typename... Args>
    explicit thread(Fn&& fn, Args&& ... args)
    {
        is_joinable = std::make_unique<bool>(true);
        ffrt::submit(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...), {}, {is_joinable.get()});
    }

    thread(thread&& th) noexcept
    {
        swap(th);
    }

    thread& operator=(thread&& th) noexcept
    {
        if (this != &th) {
            thread tmp(std::move(th));
            swap(tmp);
        }
        return *this;
    }

    bool joinable() const noexcept
    {
        return is_joinable.get() && *is_joinable;
    }

    void detach() noexcept
    {
        is_joinable = nullptr;
    }

    void join() noexcept
    {
        ffrt::wait({is_joinable.get()});
        *is_joinable = false;
    }

    ~thread()
    {
        if (joinable()) {
            std::terminate();
        }
    }

private:
    void swap(thread& other) noexcept
    {
        is_joinable.swap(other.is_joinable);
    };
    std::unique_ptr<bool> is_joinable = nullptr;
};
} // namespace ffrt
#endif
