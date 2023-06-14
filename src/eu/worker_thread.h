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

#ifndef FFRT_WORKER_THREAD_HPP
#define FFRT_WORKER_THREAD_HPP

#include <atomic>
#include <thread>

#include "sched/qos.h"
#include "core/task_ctx.h"

namespace ffrt {
class WorkerThread {
public:
    TaskCtx* curTask = nullptr;
    explicit WorkerThread(const QoS& qos) : exited(false), idle(false), tid(-1), qos(qos)
    {
    }

    virtual ~WorkerThread()
    {
        Join();
    }

    bool Idle() const
    {
        return idle;
    }

    void SetIdle(bool var)
    {
        this->idle = var;
    }

    bool Exited() const
    {
        return exited;
    }

    void SetExited(bool var)
    {
        this->exited = var;
    }

    pid_t Id() const
    {
        while (!exited && tid < 0) {
        }
        return tid;
    }

    const QoS& GetQos() const
    {
        return qos;
    }

    template <typename F, typename... Args>
    void Start(F&& f, Args&&... args)
    {
        auto wrap = [&](Args&&... args) {
            NativeConfig();
            return f(args...);
        };
        thread = std::thread(wrap, args...);
    }

    void Join()
    {
        if (thread.joinable()) {
            thread.join();
        }
        tid = -1;
    }

    void Detach()
    {
        if (thread.joinable()) {
            thread.detach();
        }
        tid = -1;
    }

    std::thread& GetThread()
    {
        return this->thread;
    }

    void WorkerSetup(WorkerThread* wthread, const QoS& qos);
private:
    void NativeConfig();

    std::atomic_bool exited;
    std::atomic_bool idle;

    std::atomic<pid_t> tid;

    QoS qos;
    std::thread thread;
};
} // namespace ffrt
#endif
