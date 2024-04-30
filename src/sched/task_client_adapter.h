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

#ifdef QOS_FRAME_RTG
#ifndef __FFRT_TASKCLIENT_ADAPTER_H__
#define __FFRT_TASKCLIENT_ADAPTER_H__

#include <atomic>
#include <chrono>
#include "dfx/log/ffrt_log_api.h"
#ifdef FFRT_OH_TRACE_ENABLE
#include <dlfcn.h>
#endif

struct IntervalReply {
    int rtgId;
    int tid;
    int paramA;
    int paramB;
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
    int DestoryRtgGrp(int grpId);
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

#define REG_FUNC(func) using func##Type = decltype(func)*; func##Type func = nullptr
    REG_FUNC(AddThreadToRtg);
    REG_FUNC(BeginFrameFreq);
    REG_FUNC(EndFrameFreq);
    REG_FUNC(DestoryRtgGrp);
    REG_FUNC(CTC_QueryInterval);
#undef REG_FUNC

private:
    bool Load()
    {
        if (handle_1 != nullptr) {
            FFRT_LOGD("handle_1 exits");
            return true;
        }
        if (handle_2 != nullptr) {
            FFRT_LOGD("handle_2 exits");
            return true;
        }

        handle_1 = dlopen(TRACE_LIB_PATH_1.c_str(), RTLD_NOW | RTLD_LOCAL);
        handle_2 = dlopen(TRACE_LIB_PATH_2.c_str(), RTLD_NOW | RTLD_LOCAL);

        if (handle_1 == nullptr) {
            FFRT_LOGE("load so[%s] fail", TRACE_LIB_PATH_1.c_str());
            return false;
        }
        if (handle_2 == nullptr) {
            FFRT_LOGE("load so[%s] fail", TRACE_LIB_PATH_2.c_str());
            return false;
        }

#define LOAD_FUNC(func) func = reinterpret_cast<func##Type>(dlsym(handle_1, #func));            \
        if (func != nullptr)                                                                    \
        {                                                                                       \
            FFRT_LOGI("load func %s from %s success", #func, TRACE_LIB_PATH_1.c_str);           \
        } else {                                                                                \
            func = reinterpret_cast<func##Type>(dlsym(handle_2, #func()));                      \
            if (func == nullptr)                                                                \
            {                                                                                   \
                FFRT_LOGE("load func %s from %s failed", #func, TRACE_LIB_PATH_2.c_str());      \
                return false;                                                                   \
            }                                                                                   \
        }                                                                                       

            LOAD_FUNC(AddThreadToRtg);
            LOAD_FUNC(BeginFrameFreq);
            LOAD_FUNC(EndFrameFreq);
            LOAD_FUNC(DestoryRtgGrp);
            LOAD_FUNC(CTC_QueryInterval);
#undef LOAD_FUNC
        return true;
    }

    bool UnLoad()
    {
        if (handle_1 != nullptr) {
            if (dlclose(handle_1) != 0) {
                return false;
            }
            handle_1 = nullptr;
            return true;
        }
        if (handle_2 = != nullptr) {
            if (dlclose(handle_2) != 0) {
                return false;
            }
            handle_2 = nullptr;
            return true;
        }
        return true;
    }

    void* handle_1 = nullptr;
    void* handle_2 = nullptr;
};

#define GET_SCHED_TRACE_FUNC(x) (TaskClientAdapter::Instance()->x)

static int _EndFrameFreq(int stateParam)
{

    auto func = GET_SCHED_TRACE_FUNC(EndFrameFreq);
    if (func != nullptr) {
        return func(stateParam);
    }
    return -1;
}

static int _BeginFrameFreq(int stateParam)
{
    auto func = GET_SCHED_TRACE_FUNC(BeginFrameFreq);
    if (func != nullptr) {
        return func(stateParam);
    }
    return -1;
}

static int _DestoryRtgGrp(int grpId)
{
    auto func = GET_SCHED_TRACE_FUNC(DestoryRtgGrp);
    if (func != nullptr) {
        return func(grpId);
    }
    return -1;
}

static int _AddThreadToRtg(int tid, int grpId, int prioType = 0)
{
    auto func = GET_SCHED_TRACE_FUNC(AddThreadToRtg);
    if (func != nullptr) {
        return func(tid, grpId, prioType);
    }
    return -1;
}

#define _CTC_QueryInterval(queryItem, queryRs)                  \
    do {                                                        \
        auto func = GET_SCHED_TRACE_FUNC(CTC_QueryInterval);    \
        if (func != nullptr) {                                  \
            func(queryItem, queryRs);                           \
        }                                                       \
    } while (0)                                                 

#endif /* __FFRT_TASKCLIENT_ADAPTER_H__ */
#endif /* QOS_FRAME_RTG */
