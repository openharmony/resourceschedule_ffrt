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
#include <unistd.h>
#ifdef FFRT_PTHREAD_ENABLE
#include <pthread.h>
#endif
#include <thread>
#ifdef OHOS_THREAD_STACK_DUMP
#include <sstream>
#include "dfx_dump_catcher.h"
#endif

#include "qos.h"
#include "tm/cpu_task.h"
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
constexpr int PTHREAD_CREATE_NO_MEM_CODE = 11;
constexpr int FFRT_RETRY_MAX_COUNT = 12;
const std::vector<uint64_t> FFRT_RETRY_CYCLE_LIST = {
    10 * 1000, 50 * 1000, 100 * 1000, 200 * 1000, 500 * 1000, 1000 * 1000, 2 * 1000 * 1000,
    5 * 1000 * 1000, 10 * 1000 * 1000, 50 * 1000 * 1000, 100 * 1000 * 1000, 500 * 1000 * 1000
};

class WorkerThread {
public:
    TaskBase* curTask = nullptr;

    uintptr_t curTaskType_ = ffrt_invalid_task;
    std::string curTaskLabel_ = ""; // 需要打开宏WORKER_CACHE_NAMEID才会赋值
    uint64_t curTaskGid_ = UINT64_MAX;
    explicit WorkerThread(const QoS& qos);

    virtual ~WorkerThread()
    {
        if (!exited) {
#ifdef OHOS_THREAD_STACK_DUMP
            FFRT_LOGW("WorkerThread enter destruction but not exited");
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
        FFRT_LOGI("to exit, qos[%d]", qos());
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
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    unsigned int GetDomainId() const
    {
        return domain_id;
    }
#endif

#ifdef FFRT_PTHREAD_ENABLE
    void Start(void*(*ThreadFunc)(void*), void* args)
    {
        int ret = pthread_create(&thread_, &attr_, ThreadFunc, args);
        if (ret == PTHREAD_CREATE_NO_MEM_CODE) {
            int count = 0;
            while (ret == PTHREAD_CREATE_NO_MEM_CODE && count < FFRT_RETRY_MAX_COUNT) {
                usleep(FFRT_RETRY_CYCLE_LIST[count]);
                count++;
                FFRT_LOGW("pthread_create failed due to shortage of system memory, FFRT retry %d times...", count);
                ret = pthread_create(&thread_, &attr_, ThreadFunc, args);
            }
        }
        if (ret != 0) {
            FFRT_LOGE("pthread_create failed, ret = %d", ret);
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

    void WorkerSetup(WorkerThread* wthread);
    void NativeConfig();
    void* worker_mgr;

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
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    unsigned int domain_id;
#endif
};
void SetThreadAttr(WorkerThread* thread, const QoS& qos);
} // namespace ffrt
#endif
