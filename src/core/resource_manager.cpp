/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include <dlfcn.h>
#include <c/type_def_ext.h>
#include <dfx/log/ffrt_log_api.h>
#include <internal_inc/osal.h>

using namespace ffrt;

namespace {
const std::string HIAI_LIB_PATH = "libhiai_foundation.so";
constexpr uint32_t BW_LOW = 3;
constexpr uint32_t BW_HIGH = 3;
constexpr uint32_t BW_MEDIUM = 3;
constexpr uint32_t BW_ZERO = 3;

extern "C" {
    int HMS_HiAIConfig_SetComputeIOBandWidthMode(uint32_t pid, uint32_t modelId, uint32_t bwMode);
}

enum llm_perf_mode {
    llm_unset = 0,
    llm_low,
    llm_middle,
    llm_high,
    llm_extreme_high,
};

class HiaiAdapter {
public:
    HiaiAdapter()
    {
        handle = dlopen(HIAI_LIB_PATH.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            FFRT_LOGE("dlopen %s failed: %s", HIAI_LIB_PATH.c_str(), dlerror());
            return;
        }

#define LOAD_FUNC(func) func##Func = reinterpret_cast<func##Type>(dlsym(handle, #func))      \
if (func##Func == nullptr)                                                                   \
        {                                                                                    \
            FFRT_LOGE("dlsym %s failed: %s", #func, dlerror());                              \
            return;                                                                          \
        }
        LOAD_FUNC(HMS_HiAIConfig_SetComputeIOBandWidthMode);
#undef LOAD_FUNC
    }

    ~HiaiAdapter()
    {
        if (handle != nullptr) {
            if (dlclose(handle) != 0) {
                FFRT_LOGE("dlclose %s failed: %s", HIAI_LIB_PATH.c_str(), dlerror());
            }
            handle = nullptr;
        }
    }

    static HiaiAdapter* Instance()
    {
        static HiaiAdapter instance;
        return &instance;
    }
#define REG_FUNC(func) using func##Func = decltype(func)*; func##Type func##Func = nullptr
    REG_FUNC(HMS_HiAIConfig_SetComputeIOBandWidthMode);
#undef REG_FUNC

private:
    void* handle = nullptr;
};
}

extern "C" {
/**
 * @brief 该接口用于设置任务的IO QoS（Quality of Service）参数，以控制任务的带宽使用情况
 *
 * @param service 服务，目前只支持值为0
 * @param id modelId
 * @param qos 降速档位，取值范围为0-4，分别对应不同的带宽使用限制
 * @param payload 保留参数，暂无作用
 * @return 调用成功，返回值位0，否则返回值为1
 */
API_ATTRIBUTE((visibility("default")))
int ffrt_set_task_io_qos(int service, uint64_t id, int qos, void* payload)
{
    if (service != 0) {
        FFRT_LOGE("unknown service %d", service);
        return 1;
    }

    constexpr uint32_t qos2bw[] = {BW_LOW, BW_HIGH, BW_MEDIUM, BW_LOW, BW_ZERO};
    if (qos < llm_unset || qos > llm_extreme_high) {
        FFRT_LOGE("invalid qos %d", qos);
        return 1;
    }

    static auto func = HiaiAdapter::Instance()->HMS_HiAIConfig_SetComputeIOBandWidthModeFunc;
    if (func != nullptr) {
        return func(getpid(), id, qos2bw[qos]);
    }

    return 0;
}
}