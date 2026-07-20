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

#ifndef __FFRT_TRACE_H__
#define __FFRT_TRACE_H__

#include <atomic>
#include <chrono>
#include <vector>
#include <securec.h>
#include "internal_inc/osal.h"
#include "internal_inc/types.h"
#include "dfx/log/ffrt_log_api.h"
#include <linux/perf_event.h>

#ifdef FFRT_OH_TRACE_ENABLE
#include <dlfcn.h>
#endif

namespace ffrt {
constexpr int MAX_PERF_COUNTERS = 8;

enum Module {
    CUSTOM = 0, // user define module
    EU,
    SCHED,
    DM,
    QUEUE,
    SYNC,
    MODULE_MAX,
};

enum ConfigGroup {
    DEFAULT_CONFIG = 0, // cycle + instructions
    CYCLES,             // cycles
    CONFIG_MAX,
};

struct PerfEventConfig {
    perf_hw_id event;
    std::string name;
};

struct Counters {
    unsigned long prefix;
    unsigned long perfCnt[MAX_PERF_COUNTERS];
};

struct PerfStatus {
    int groupFd;
    int fds[MAX_PERF_COUNTERS];
    struct Counters eventCounters;
    std::string eventNames[MAX_PERF_COUNTERS];
    int counterNum;
};

class PerfTraceScoped {
public:
    PerfTraceScoped(Module module, const std::string& name, const std::vector<PerfEventConfig>& configs);
    PerfTraceScoped(Module module, const std::string& name, ConfigGroup group);
    ~PerfTraceScoped();

    static void SetEnable();
    static void SetPerfInitOnce();

private:
    void PerfInit(const std::vector<PerfEventConfig>& configs);

private:
    std::string mName_;
    struct PerfStatus status_ = {.groupFd = -1};
    int err_ = 0;
    static std::atomic<bool> moduleEnabled_[MODULE_MAX];
    static pthread_once_t perfInitOnce_;
};

enum TraceLevel {
    TRACE_LEVEL0 = 0,
    TRACE_LEVEL1,
    TRACE_LEVEL2,
    TRACE_LEVEL3, // lowest level, trace all
    TRACE_LEVEL_MAX,
};

class TraceLevelManager {
public:
    TraceLevelManager();
    ~TraceLevelManager() = default;

    uint64_t GetTraceLevel() const
    {
        return traceLevel_;
    }

    static inline TraceLevelManager* Instance()
    {
        static TraceLevelManager ins;
        return &ins;
    }

private:
    uint8_t traceLevel_;
};

class ScopedTrace {
public:
    ScopedTrace(uint64_t level, const char* name);
    ~ScopedTrace();

private:
    std::atomic<bool> isTraceEnable_;
};
} // namespace ffrt

#ifdef FFRT_ENABLE_PERF_TRACE_SCOPED
#define FFRT_PERF_TRACE_SCOPED_BY_GROUP(module, name, configGroup) \
    ffrt::PerfTraceScoped ___perfTracer##name(module, #name, configGroup)
#define FFRT_PERF_TRACE_SCOPED_BY_CONFIG(module, name, configs) \
    ffrt::PerfTraceScoped ___perfTracer##name(module, #name, configs)
#else
#define FFRT_PERF_TRACE_SCOPED_BY_GROUP(module, name, configGroup)
#define FFRT_PERF_TRACE_SCOPED_BY_CONFIG(module, name, configs)
#endif

#ifdef FFRT_OH_TRACE_ENABLE
constexpr uint64_t HITRACE_TAG_FFRT = (1ULL << 13); // ffrt tasks.
bool IsTagEnabled(uint64_t tag);
void StartTrace(uint64_t label, const std::string& value, float limit = -1);
void FinishTrace(uint64_t label);
void StartAsyncTrace(uint64_t label, const std::string& value, int32_t taskId, float limit = -1);
void FinishAsyncTrace(uint64_t label, const std::string& value, int32_t taskId);
void CountTrace(uint64_t label, const std::string& name, int64_t count);

static inline bool IsTagEnabledStub(uint64_t){return false;}
static inline void StartTraceStub(uint64_t, const std::string&, float) {}
static inline void FinishTraceStub(uint64_t) {}
static inline void StartAsyncTraceStub(uint64_t, const std::string&, int32_t, float) {}
static inline void FinishAsyncTraceStub(uint64_t, const std::string&, int32_t) {}
static inline void CountTraceStub(uint64_t, const std::string&, int64_t) {}

#define REG_FUNC(func) using func##Type = decltype(func)*; inline func##Type g##func = func##Stub
    REG_FUNC(IsTagEnabled);
    REG_FUNC(StartTrace);
    REG_FUNC(FinishTrace);
    REG_FUNC(StartAsyncTrace);
    REG_FUNC(FinishAsyncTrace);
    REG_FUNC(CountTrace);
#undef REG_FUNC

constexpr const char* TRACE_LIB_PATH = "libhitrace_meter.so";
class TraceAdapter {
public:
    TraceAdapter()
    {
        Load();
    }

    ~TraceAdapter()
    {
        UnLoad();
    }

    static TraceAdapter* Instance()
    {
        static TraceAdapter instance;
        return &instance;
    }

private:
    bool Load()
    {
        if (handle != nullptr) {
            FFRT_LOGD("handle exits");
            return true;
        }

        handle = dlopen(TRACE_LIB_PATH, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
        if (handle == nullptr) {
            FFRT_LOGE("load so[%s] fail: %s", TRACE_LIB_PATH, dlerror());
            return false;
        }

#define LOAD_FUNC(x)                                                           \
    do {                                                                       \
        auto func = reinterpret_cast<x##Type>(dlsym(handle, #x));              \
        if (func == nullptr) {                                                 \
            FFRT_LOGE("load func %s from %s failed, use stub",                 \
                #x, TRACE_LIB_PATH);                                           \
        } else {                                                               \
            g##x = func;                                                       \
        }                                                                      \
    } while (0)
            LOAD_FUNC(IsTagEnabled);
            LOAD_FUNC(StartTrace);
            LOAD_FUNC(FinishTrace);
            LOAD_FUNC(StartAsyncTrace);
            LOAD_FUNC(FinishAsyncTrace);
            LOAD_FUNC(CountTrace);
#undef LOAD_FUNC
        return true;
    }

    bool UnLoad()
    {
        if (handle != nullptr) {
            if (dlclose(handle) != 0) {
                return false;
            }
            handle = nullptr;
            return true;
        }
        return true;
    }

    void* handle = nullptr;
};

#define GET_TRACE_FUNC(x) (g##x)

static bool _IsTagEnabled(uint64_t label)
{
    return GET_TRACE_FUNC(IsTagEnabled)(label);
}

#define _StartTrace(label, tag, limit) \
    do { \
        GET_TRACE_FUNC(StartTrace)(label, tag, limit); \
    } while (0)

#define _FinishTrace(label) \
    do { \
        GET_TRACE_FUNC(FinishTrace)(label); \
    } while (0)

#define _StartAsyncTrace(label, tag, tid, limit) \
    do { \
        GET_TRACE_FUNC(StartAsyncTrace)(label, tag, tid, limit); \
    } while (0)

#define _FinishAsyncTrace(label, tag, tid) \
    do { \
        GET_TRACE_FUNC(FinishAsyncTrace)(label, tag, tid); \
    } while (0)

#define _TraceCount(label, tag, value) \
    do { \
        GET_TRACE_FUNC(CountTrace)(label, tag, value); \
    } while (0)

#define FFRT_TRACE_BEGIN(tag) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            _StartTrace(HITRACE_TAG_FFRT, tag, -1); \
    } while (false)
#define FFRT_TRACE_END() \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            _FinishTrace(HITRACE_TAG_FFRT); \
    } while (false)
#define FFRT_TRACE_ASYNC_BEGIN(tag, tid) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            _StartAsyncTrace(HITRACE_TAG_FFRT, tag, tid, -1); \
    } while (false)
#define FFRT_TRACE_ASYNC_END(tag, tid) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            _FinishAsyncTrace(HITRACE_TAG_FFRT, tag, tid); \
    } while (false)
#define FFRT_TRACE_COUNT(tag, value) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            _TraceCount(HITRACE_TAG_FFRT, tag, value); \
    } while (false)

static inline FFRT_NOINLINE void FFRTSubmitMarkerSlowPath(uint64_t gid)
{
    char buf[128];
    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "P %llu", gid);
    _StartTrace(HITRACE_TAG_FFRT, buf, -1);
}

static inline FFRT_NOINLINE void FFRTTaskBeginSlowPath(const std::string& tag, uint64_t gid)
{
    char buf[128];
    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "FFRT%s|%llu", tag.c_str(), gid);
    _StartTrace(HITRACE_TAG_FFRT, buf, -1);
}

static inline FFRT_NOINLINE void FFRTBlockTracerSlowPath(uint64_t gid, const std::string& tag)
{
    char buf[128];
    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "FFBK%s|%llu", tag.c_str(), gid);
    _StartTrace(HITRACE_TAG_FFRT, buf, -1);
}

static inline FFRT_NOINLINE void FFRTWakeTracerSlowPath(uint64_t gid)
{
    char buf[128];
    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "FFWK|%llu", gid);
    _StartTrace(HITRACE_TAG_FFRT, buf, -1);
}

static inline FFRT_NOINLINE void FFRTExecutorTaskBeginSlowPath(uint64_t gid)
{
    char buf[128];
    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "FFRTex_task|%llu", gid);
    _StartTrace(HITRACE_TAG_FFRT, buf, -1);
}

static inline FFRT_NOINLINE void FFRTSerialQueueTaskSubmitSlowPath(uint64_t qid, uint64_t gid)
{
    char buf[128];
    snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "P[sq_%llu]|%llu", qid, gid);
    _StartTrace(HITRACE_TAG_FFRT, buf, -1);
}

#define FFRT_SUBMIT_MARKER(gid) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) { \
            FFRTSubmitMarkerSlowPath(gid); \
        } \
    } while (false)
#define FFRT_TASK_BEGIN(tag, gid) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) { \
            FFRTTaskBeginSlowPath(tag, gid); \
        } \
    } while (false)
#define FFRT_BLOCK_TRACER(gid, tag) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) { \
            FFRTBlockTracerSlowPath(gid, #tag); \
        } \
        FFRT_TRACE_END(); \
    } while (false)
#define FFRT_WAKE_TRACER(gid) \
    do { \
        if FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT)) { \
            FFRTWakeTracerSlowPath(gid); \
        } \
        FFRT_TRACE_END(); \
    } while (false)
#define FFRT_EXECUTOR_TASK_BEGIN(gid) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) { \
            FFRTExecutorTaskBeginSlowPath(gid); \
        } \
    } while (false)
#define FFRT_SERIAL_QUEUE_TASK_SUBMIT_MARKER(qid, gid) \
    do { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) { \
            FFRTSerialQueueTaskSubmitSlowPath(qid, gid); \
        } \
        FFRT_TRACE_END(); \
    } while (false)
#define FFRT_TRACE_SCOPE(level, tag) ffrt::ScopedTrace ___tracer##tag(level, #tag)
#else
#define FFRT_TRACE_BEGIN(tag)
#define FFRT_TRACE_END()
#define FFRT_TRACE_ASYNC_BEGIN(tag, tid)
#define FFRT_TRACE_ASYNC_END(tag, tid)
#define FFRT_TRACE_COUNT(tag, value)
#define FFRT_TRACE_SCOPE(level, tag)
#define FFRT_SUBMIT_MARKER(gid)
#define FFRT_TASK_BEGIN(tag, gid)
#define FFRT_BLOCK_TRACER(gid, tag)
#define FFRT_WAKE_TRACER(gid)
#define FFRT_EXECUTOR_TASK_BEGIN(gid)
#define FFRT_SERIAL_QUEUE_TASK_SUBMIT_MARKER(qid, gid)
#endif

// DFX Trace for FFRT Normal Task
#define FFRT_WORKER_IDLE_BEGIN_MARKER()
#define FFRT_WORKER_IDLE_END_MARKER()

#ifdef FFRT_OH_TRACE_ENABLE
static inline FFRT_NOINLINE void FFRTReadyMarkerSlowPath(uint64_t gid)
{
    _FinishAsyncTrace(HITRACE_TAG_FFRT, "R", gid);
}

static inline FFRT_NOINLINE void FFRTBlockMarkerSlowPath(uint64_t gid)
{
    _FinishAsyncTrace(HITRACE_TAG_FFRT, "B", gid);
}

static inline FFRT_NOINLINE void FFRTTaskDoneMarkerSlowPath(uint64_t gid)
{
    _FinishAsyncTrace(HITRACE_TAG_FFRT, "F", gid);
}

static inline FFRT_NOINLINE void FFRTExecutorTaskSubmitMarkerSlowPath(uint64_t gid)
{
    _FinishAsyncTrace(HITRACE_TAG_FFRT, "P", gid);
}
#endif

#ifdef FFRT_OH_TRACE_ENABLE
#define FFRT_READY_MARKER(gid) \
    { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            FFRTReadyMarkerSlowPath(gid); \
    }
#define FFRT_BLOCK_MARKER(gid) \
    { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            FFRTBlockMarkerSlowPath(gid); \
    }
#define FFRT_TASKDONE_MARKER(gid) \
    { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            FFRTTaskDoneMarkerSlowPath(gid); \
    }
// DFX Trace for FFRT Executor Task
#define FFRT_EXECUTOR_TASK_SUBMIT_MARKER(gid) \
    { \
        if (FFRT_UNLIKELY(_IsTagEnabled(HITRACE_TAG_FFRT))) \
            FFRTExecutorTaskSubmitMarkerSlowPath(gid); \
    }
#else
#define FFRT_READY_MARKER(gid)
#define FFRT_BLOCK_MARKER(gid)
#define FFRT_TASKDONE_MARKER(gid)
#define FFRT_EXECUTOR_TASK_SUBMIT_MARKER(gid)
#endif

#define FFRT_TASK_END() \
    { \
        FFRT_TRACE_END(); \
    }

#endif