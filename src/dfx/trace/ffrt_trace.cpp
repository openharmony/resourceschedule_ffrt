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

#include <sys/ioctl.h>
#include "internal_inc/osal.h"
#include "securec.h"
#include "pthread.h"
#include "util/white_list.h"
#include "dfx/trace/ffrt_trace.h"

namespace ffrt {
static const std::string moduleList[MODULE_MAX] = {
    "PerfTraceScopedCustom",
    "PerfTraceScopedEu",
    "PerfTraceScopedSched",
    "PerfTraceScopedDm",
    "PerfTraceScopedQueue",
    "PerfTraceScopedSync"
};

static const std::vector<PerfEventConfig> configGroupList[CONFIG_MAX] = {
    {
        {PERF_COUNT_HW_CPU_CYCLES, "cycles"},
        {PERF_COUNT_HW_INSTRUCTIONS, "instructions"},
    }, // DEFAULT_CONFIG
    {
        {PERF_COUNT_HW_CPU_CYCLES, "cycles"},
        {PERF_COUNT_HW_BUS_CYCLES, "bus-cycles"},
        {PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, "stalled-cycles-frontend"},
        {PERF_COUNT_HW_STALLED_CYCLES_BACKEND, "stalled-cycles-backend"},
        {PERF_COUNT_HW_REF_CPU_CYCLES, "ref-cycles"},

    } // CYCLES
};

std::atomic<bool> PerfTraceScoped::moduleEnabled_[MODULE_MAX] = {};
pthread_once_t PerfTraceScoped::perfInitOnce_ = PTHREAD_ONCE_INIT;
PerfTraceScoped::PerfTraceScoped(Module module, const std::string& name, const std::vector<PerfEventConfig>& configs)
    :mName_(name), err_(0)
{
    pthread_once(&perfInitOnce_, SetEnable);
    if (!moduleEnabled_[module]) {
        return;
    }

    if (configs.empty() || configs.size() > MAX_PERF_COUNTERS) {
        err_ = -1;
        FFRT_LOGW("Config is invalid!");
    } else {
        PerfInit(configs);
    }
}

PerfTraceScoped::PerfTraceScoped(Module module, const std::string& name, ConfigGroup group)
    :mName_(name), err_(0)
{
    pthread_once(&perfInitOnce_, SetEnable);
    if (!moduleEnabled_[module]) {
        return;
    }

    if (group >= DEFAULT_CONFIG && group < CONFIG_MAX) {
        PerfInit(configGroupList[group]);
    } else {
        FFRT_LOGW("Config group is invalid!");
    }
}

PerfTraceScoped::~PerfTraceScoped()
{
    if (status_.groupFd != -1) {
        ioctl(status_.groupFd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
        read(status_.groupFd, &(status_.eventCounters), sizeof(unsigned long) * (status_.counterNum + 1));
        for (int k = 0; k < status_.counterNum; k++) {
            close(status_.fds[k]);
            FFRT_TRACE_COUNT("PerfTraceScoped: "+ mName_ + "-" + status_.eventNames[k],
                status_.eventCounters.perfCnt[k]);
        }
    }
}

void PerfTraceScoped::SetEnable()
{
    if (WhiteList::GetInstance().IsEnabled("PerfTraceScopedAll", false)) {
        for (int i = 0; i < MODULE_MAX; i++) {
            moduleEnabled_[i] = true;
        }
        FFRT_LOGI("Set all modules enable");
    } else {
        for (int i = 0; i < MODULE_MAX; i++) {
            moduleEnabled_[i] = WhiteList::GetInstance().IsEnabled(moduleList[i], false);
            if (moduleEnabled_[i]) {
                FFRT_LOGI("Set %s enable", moduleList[i].c_str());
            }
        }
    }
}

void PerfTraceScoped::SetPerfInitOnce()
{
    perfInitOnce_ = PTHREAD_ONCE_INIT;
}

void PerfTraceScoped::PerfInit(const std::vector<PerfEventConfig>& configs)
{
    for (const auto& config : configs) {
        struct perf_event_attr pe;
        if (memset_s(&pe, sizeof(struct perf_event_attr), 0, sizeof(struct perf_event_attr)) != EOK) {
            err_ = -1;
            FFRT_LOGE("Failed to init perf_event_attr for %s", config.name.c_str());
            return;
        }
        pe.type = PERF_TYPE_HARDWARE;
        pe.size = sizeof(struct perf_event_attr);
        pe.config = config.event;
        pe.disabled = 1;
        pe.exclude_kernel = 0;
        pe.exclude_hv = 0;
        pe.read_format = PERF_FORMAT_GROUP;

        int fd = syscall(__NR_perf_event_open, &pe, 0, -1, status_.groupFd, 0);
        if (fd == -1) {
            FFRT_LOGE("Failed to syscall __NR_perf_event_open for %s, error %s", config.name.c_str(), strerror(errno));
            err_ = -1;
            return;
        }

        if (status_.groupFd == -1) {
            status_.groupFd = fd;
        }

        status_.fds[status_.counterNum] = fd;
        status_.eventNames[status_.counterNum] = config.name;
        status_.counterNum++;
    }

    ioctl(status_.groupFd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    ioctl(status_.groupFd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
}

TraceLevelManager::TraceLevelManager()
{
    traceLevel_ = FFRT_TRACE_LEVEL;
    std::string trace = GetEnv("FFRT_TRACE_LEVEL");
    if (trace.size() == 0) {
        return;
    }

    int traceLevel = std::stoi(trace);
    if (traceLevel >= TRACE_LEVEL_MAX || traceLevel < TRACE_LEVEL0) {
        FFRT_LOGE("get invalid trace level, %d", traceLevel);
        return;
    }
    traceLevel_ = static_cast<uint8_t>(traceLevel);
    FFRT_LOGW("set trace level %d", traceLevel_);
}

// make sure TraceLevelManager last free
static __attribute__((constructor)) void TraceInit(void)
{
    ffrt::TraceLevelManager::Instance();
}

ScopedTrace::ScopedTrace(uint64_t level, const char* name)
    : isTraceEnable_(false)
{
    if (level <= TraceLevelManager::Instance()->GetTraceLevel()) {
        isTraceEnable_ = true;
        FFRT_TRACE_BEGIN(name);
    }
}

ScopedTrace::~ScopedTrace()
{
    if (!isTraceEnable_) {
        return;
    }

    FFRT_TRACE_END();
}
} // namespace ffrt