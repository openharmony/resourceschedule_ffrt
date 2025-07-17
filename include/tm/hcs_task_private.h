/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef HCS_TASK_PRIVATE_H
#define HCS_TASK_PRIVATE_H

#include <atomic>
#include <dlfcn.h>
#include "c/ffrt_types.h"
#include "c/ffrt_static_graph.h"
#include "cpp/task_ext.h"
#include "util/ref_function_header.h"
#include "cpp/shared_mutex.h"

namespace ffrt {
#ifdef OHOS_STANDARD_SYSTEM
static const char* LIB_STATIC_SCHEDULER_CLIENT_PATH = "libstatic_scheduler_client.so";
#else
static const char* LIB_STATIC_SCHEDULER_CLIENT_PATH = "/vendor/lib64/libstatic_scheduler_client.so";
#endif
typedef int (*ffrt_alloc_event_func)(device dev, uint16_t *events, uint32_t num, uint32_t* event_handle);
typedef int (*ffrt_free_event_func)(uint32_t event_handle);

class StaticSchedClientLibraryManager {
public:
    StaticSchedClientLibraryManager()
    {
        staticClientHandle_ = dlopen(LIB_STATIC_SCHEDULER_CLIENT_PATH, RTLD_LAZY | RTLD_NODELETE);
        if (staticClientHandle_ != nullptr) {
            ffrtAllocEventFunc_ = (ffrt_alloc_event_func)dlsym(staticClientHandle_, "ffrt_alloc_event");
            ffrtFreeEventFunc_ = (ffrt_free_event_func)dlsym(staticClientHandle_, "ffrt_free_event");
        }
    }
    ~StaticSchedClientLibraryManager()
    {
        dlclose(staticClientHandle_);
    }

    static StaticSchedClientLibraryManager& GetInstance()
    {
        static StaticSchedClientLibraryManager instance;
        return instance;
    }

    ffrt_alloc_event_func ffrtAllocEventFunc_ = nullptr;
    ffrt_free_event_func ffrtFreeEventFunc_ = nullptr;
private:
    void* staticClientHandle_ = nullptr;
};

class EventPair {
public:
    EventPair(device dev)
    {
        ffrt_alloc_event_func allocEventFunc = StaticSchedClientLibraryManager::GetInstance().ffrtAllocEventFunc_;
        if (allocEventFunc != nullptr) {
            int ret = allocEventFunc(dev, events, 2, &eventHandle_);
            FFRT_COND_DO_ERR((ret != 0), return, "ffrt_alloc_event failed");
            allocEventSucc_ = true;
        } else {
            FFRT_LOGE("allocEventFunc is null");
        }
    }

    ~EventPair()
    {
        if (allocEventSucc_) {
            ffrt_free_event_func freeEventFunc = StaticSchedClientLibraryManager::GetInstance().ffrtFreeEventFunc_;
            if (freeEventFunc != nullptr) {
                freeEventFunc(eventHandle_);
            } else {
                FFRT_LOGE("freeEventFunc is null");
            }
        }
    }

    inline void IncDeleteRef()
    {
        rc.fetch_add(1);
    }

    inline void DecDeleteRef()
    {
        auto v = rc.fetch_sub(1);
        if (v == 1) {
            delete this;
        }
    }

    uint16_t events[2] = {0};
private:
    uint32_t eventHandle_ = 0;
    std::atomic_uint16_t rc = 1;
    bool allocEventSucc_ = false;
};

class HcsTaskPrivate {
public:
    HcsTaskPrivate(uint32_t type)
    {
        dev_ = static_cast<device>(type & 0xFFFF);
        useEvent_ = type & ffrt_hcs_interface_event;

        if (useEvent_) {
            eventPair_ = new EventPair(dev_);
        }
    }

    ~HcsTaskPrivate()
    {
        if (run_) {
            run_->DecDeleteRef();
        }
        if (preRun_) {
            preRun_->DecDeleteRef();
        }
        if (postRun_) {
            postRun_->DecDeleteRef();
        }
        if (useEvent_ && eventPair_) {
            if (!isRefEvent_) {
                delete eventPair_;
            } else {
                eventPair_->DecDeleteRef();
            }
        }
    }

    inline uint16_t GetWaitEvent() const
    {
        if (!eventPair_) {
            return 0;
        }
        return eventPair_->events[0];
    }

    inline uint16_t GetNotifyEvent() const
    {
        if (!eventPair_) {
            return 0;
        }
        return eventPair_->events[1];
    }

    device dev_ = dev_min;
    bool useEvent_ = false;
    bool isRefEvent_ = true;
    bool funcptrIsSet_ = false;
    // 通路3
    EventPair* eventPair_ = nullptr;
    // 通路2，函数闭包、指针或函数对象
    ffrt_function_t funcptr_ = nullptr;
    RefFunctionHeader* run_ = nullptr;
    RefFunctionHeader* preRun_ = nullptr;
    RefFunctionHeader* postRun_ = nullptr;
    ffrt::shared_mutex mutex_;
};
}
#endif // HCS_TASK_PRIVATE_H