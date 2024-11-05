/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of License at
*
*     http://www.apache.org/license/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/


#ifndef __FFRT_TASKCLIENT_ADAPTER_H__
#define __FFRT_TASKCLIENT_ADAPTER_H__

#include <atomic>
#include <chrono>
#include <dlfcn.h>
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"


#if (defined(QOS_WORKER_FRAME_RTG) || defined(QOS_FRAME_RTG))

struct IntervalReply {
    int rtgId;
    int tid;
    int paramA;
    int paramB;
    std::string bundleName;
};

enum QueryIntervalItem {
    QUERY_UI = 0,
    QUERY_RENDER = 1,
    QUERY_RENDER_SERVICE = 2,
    QUERY_COMPOSER = 3,
    QUERY_HARDWARE = 4,
    QUERY_EXECUTOR_START = 5,
    QUERY_TYPE_MAX
};

extern "C" {
    int AddThreadToRtg(int tid, int grpId, int prioType = 0);
    int DestroyRtgGrp(int grpId);
    int BeginFrameFreq(int stateParam);
    int EndFrameFreq(int stateParam);
    void CTC_QueryInterval(int queryItem, IntervalReply& queryRs);
}

static const std::string TRACE_LIB_PATH_1 = "/system/lib64/libconcurrentsvc.z.so";
static const std::string TRACE_LIB_PATH_2 = "/system/lib64/libconcurrent_task_client.z.so";

class TaskClientAdapter {
public:
    TaskClientAdapter()
    {
        Load();
    }

    ~TaskClientAdapter()
    {
        UnLoad();
    }

    static TaskClientAdapter* Instance()
    {
        static TaskClientAdapter instance;
        return &instance;
    }

#define REG_FUNC(func) using func##Type = decltype(func)*; func##Type func##Func = nullptr
    REG_FUNC(AddThreadToRtg);
    REG_FUNC(BeginFrameFreq);
    REG_FUNC(EndFrameFreq);
    REG_FUNC(DestroyRtgGrp);
    REG_FUNC(CTC_QueryInterval);
#undef REG_FUNC

private:
    void Load()
    {
        if (handle_1 != nullptr) {
            FFRT_LOGD("handle_1 exits");
            return;
        }
        if (handle_2 != nullptr) {
            FFRT_LOGD("handle_2 exits");
            return;
        }

        handle_1 = dlopen(TRACE_LIB_PATH_1.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle_1 == nullptr) {
            FFRT_LOGE("load so[%s] fail", TRACE_LIB_PATH_1.c_str());
            return;
        }

        handle_2 = dlopen(TRACE_LIB_PATH_2.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle_2 == nullptr) {
            FFRT_LOGE("load so[%s] fail", TRACE_LIB_PATH_2.c_str());
            return;
        }

#define LOAD_FUNC(func) func##Func = reinterpret_cast<func##Type>(dlsym(handle_1, #func));      \
        if (func##Func != nullptr) {                                                            \
            FFRT_LOGI("load func %s from %s success", #func, TRACE_LIB_PATH_1.c_str());         \
        } else {                                                                                \
            func##Func = reinterpret_cast<func##Type>(dlsym(handle_2, #func));                  \
            if (func##Func == nullptr) {                                                        \
                FFRT_LOGE("load func %s from %s failed", #func, TRACE_LIB_PATH_2.c_str());      \
            }                                                                                   \
        }

            LOAD_FUNC(AddThreadToRtg);
            LOAD_FUNC(BeginFrameFreq);
            LOAD_FUNC(EndFrameFreq);
            LOAD_FUNC(DestroyRtgGrp);
            LOAD_FUNC(CTC_QueryInterval);
#undef LOAD_FUNC
    }

    void UnLoad()
    {
        if (handle_1 != nullptr) {
            if (dlclose(handle_1) != 0) {
                FFRT_LOGE("unLoad handle_1 fail");
            }
            handle_1 = nullptr;
        }
        if (handle_2 != nullptr) {
            if (dlclose(handle_2) != 0) {
                FFRT_LOGE("unLoad handle_2 fail");
            }
            handle_2 = nullptr;
        }
    }

    void* handle_1 = nullptr;
    void* handle_2 = nullptr;
};

static int EndFrameFreqAdapter(int stateParam)
{
    auto func = TaskClientAdapter::Instance()->EndFrameFreqFunc;
    if (func != nullptr) {
        return func(stateParam);
    }
    return -1;
}

static int BeginFrameFreqAdapter(int stateParam)
{
    auto func = TaskClientAdapter::Instance()->BeginFrameFreqFunc;
    if (func != nullptr) {
        return func(stateParam);
    }
    return -1;
}

static int DestroyRtgGrpAdapter(int grpId)
{
    auto func = TaskClientAdapter::Instance()->DestroyRtgGrpFunc;
    if (func != nullptr) {
        return func(grpId);
    }
    return -1;
}

static int AddThreadToRtgAdapter(int tid, int grpId, int prioType = 0)
{
    auto func = TaskClientAdapter::Instance()->AddThreadToRtgFunc;
    if (func != nullptr) {
        return func(tid, grpId, prioType);
    }
    return -1;
}

#define CTC_QUERY_INTERVAL(queryItem, queryRs)                             \
    do {                                                                   \
        auto func = TaskClientAdapter::Instance()->CTC_QueryIntervalFunc;   \
        if (func != nullptr) {                                             \
            func(queryItem, queryRs);                                      \
        }                                                                  \
    } while (0)

#endif /* __FFRT_TASKCLIENT_ADAPTER_H__ */
#endif /* QOS_FRAME_RTG */
