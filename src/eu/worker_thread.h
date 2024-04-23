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
#ifdef FFRT_PTHREAD_ENABLE
#include <pthread.h>
#endif
#ifdef OHOS_THREAD_STACK_DUMP
#include <sstream>
#include "dfx_dump_catcher.h"
#endif

#include "qos.h"
#include "tm/cpu_task.h"

namespace ffrt {
class WorkerThread {
public:
    CPUEUTask* curTask = nullptr;
    explicit WorkerThread(const QoS& qos);

    virtual ~WorkerThread()
    {
        if (!exited) {
#ifdef OHOS_THREAD_STACK_DUMP
            OHOS::HiviewDFX::DfxDumpCatcher dumplog;
            std::string msg = "";
            bool result = dumplog.DumpCatch(getpid(), gettid(), msg);
            if (result) {
                std::vector<std::string> out;
                std::stringstream ss(msg);
                std::string s;
                while (std::getline(ss, s, '\n')) {
                    out.push_back(s);
                }
                for (auto const& line: out) {
                    FFRT_LOGE("ffrt callstack %s", line.c_str());
                }
            }
#endif
        }
        Detach();
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

#ifdef FFRT_PTHREAD_ENABLE
    void Start(void*(*ThreadFunc)(void*), void* args)
    {
        int ret = pthread_create(&thread_, &attr_, ThreadFunc, args);
        if (ret != 0) {
            exited = true;
        }
        pthread_attr_destroy(&attr_);
    }

    void Join()
    {
        if (tid > 0) {
            pthread_join(thread_, nullptr);
        }
        tid = -1;
    }

    void Detach()
    {
        if (tid > 0) {
            pthread_detach(thread_);
        } else {
            FFRT_LOGD("qos %d thread not joinable.", qos());
        }
        tid = -1;
    }

    pthread_t& GetThread()
    {
        return this->thread_;
    }
#else
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
        } else {
            FFRT_LOGD("qos %d thread not joinable\n", qos());
        }
        tid = -1;
    }

    pthread_t GetThread()
    {
        return this->thread.native_handle();
    }
#endif

    virtual void SetWorkerBlocked(bool var)
    {
    }

    void WorkerSetup(WorkerThread* wthread);
    void NativeConfig();

private:
    std::atomic_bool exited;
    std::atomic_bool idle;

    std::atomic<pid_t> tid;

    QoS qos;
#ifdef FFRT_PTHREAD_ENABLE
    pthread_t thread_{0};
    pthread_attr_t attr_;
#else
    std::thread thread;
#endif
};
void SetThreadAttr(WorkerThread* thread, const QoS& qos);
} // namespace ffrt
#endif
