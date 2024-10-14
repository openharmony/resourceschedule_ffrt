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
#include <dlfcn.h>
#include <memory>
#include <vector>
#include <unordered_set>
#include <fstream>
#include "sched/task_scheduler.h"
#include "eu/co_routine.h"
#include "eu/execute_unit.h"
#include "eu/sexecute_unit.h"
#include "dm/dependence_manager.h"
#include "dm/sdependence_manager.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/osal.h"
#include "util/singleton_register.h"
#include "tm/task_factory.h"
#ifndef FFRT_PARSE_XML_USE_STUB
#include "parse_xml.h"
#endif
#include "qos.h"
#include "c/ffrt_types.h"

#ifdef __cplusplus
extern "C" {
#endif

constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
typedef int (*hffrt_init_func)(void);
typedef void (*hffrt_deinit_func)(void);

static uint32_t g_ffrt_hw_property = 0;
static void* g_ffts_handle = nullptr;
pthread_once_t g_once = PTHREAD_ONCE_INIT;

void ChildHandle();

bool IsInSFFRTList()
{
#ifdef OHOS_STANDARD_SYSTEM
    static std::unordered_set<std::string> whitelist = {"/system/bin/hilogd",
        "/system/bin/uitest"};
#else
    static std::unordered_set<std::string> whitelist = {};
#endif
    char processName[PROCESS_NAME_BUFFER_LENGTH];
    GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
    if (whitelist.find(processName) != whitelist.end()) {
        return true;
    }

    return false;
}

bool ffrt_hcs_get_capability(uint32_t hw_property_bitmap)
{
    return ((hw_property_bitmap & g_ffrt_hw_property) == 0) ? false : true;
}

void SFFRTInit()
{
    ffrt::TaskFactory::RegistCb(
        [] () -> ffrt::CPUEUTask* {
            return static_cast<ffrt::CPUEUTask*>(ffrt::SimpleAllocator<ffrt::SCPUEUTask>::AllocMem());
        },
        [] (ffrt::CPUEUTask* task) {
            ffrt::SimpleAllocator<ffrt::SCPUEUTask>::FreeMem(static_cast<ffrt::SCPUEUTask*>(task));
    },
    ffrt::SimpleAllocator<ffrt::SCPUEUTask>::getUnfreedMem,
    ffrt::SimpleAllocator<ffrt::SCPUEUTask>::LockMem,
    ffrt::SimpleAllocator<ffrt::SCPUEUTask>::UnlockMem);
    ffrt::SchedulerFactory::RegistCb(
        [] () -> ffrt::TaskScheduler* { return new ffrt::TaskScheduler{new ffrt::FIFOQueue()}; },
        [] (ffrt::TaskScheduler* schd) { delete schd; });

    CoRoutineFactory::RegistCb(
        [] (ffrt::CPUEUTask* task, bool timeOut) -> void {CoWake(task, timeOut);});

    ffrt::DependenceManager::RegistInsCb(ffrt::SDependenceManager::Instance);
    ffrt::ExecuteUnit::RegistInsCb(ffrt::SExecuteUnit::Instance);
    ffrt::FFRTScheduler::RegistInsCb(ffrt::SFFRTScheduler::Instance);
    ffrt::SetFuncQosMap(ffrt::QoSMap);
    ffrt::SetFuncQosMax(ffrt::QoSMax);
#ifndef FFRT_PARSE_XML_USE_STUB
    ffrt::ParseXml::Instance().SetParaXmlPath("/vender/etc/ffrtqos_config.xml");
#endif
}

void ForkChildHandle()
{
    pthread_atfork(nullptr, nullptr, ChildHandle);
}

__attribute__((constructor)) static void ffrt_init(void)
{
    g_ffrt_hw_property = 0;
    pthread_once(&g_once, ForkChildHandle);
    ffrt::DelayedWorker::ThreadEnvCreate();

    std::string envFFRTPath = GetEnv("FFRT_PATH_HARDWARE");
    int path = 0;
    if (envFFRTPath.size() != 0) {
        path = std::stoi(envFFRTPath);
    }
    bool enableHardware = !IsInSFFRTList();
    FFRT_LOGW("FFRT_PATH_HARDWARE: %d, process enableHardware: %d", path, enableHardware);
    if (path == 0 && !enableHardware) {
        SFFRTInit();
        return;
    }
#ifdef OHOS_STANDARD_SYSTEM
    g_ffts_handle = dlopen("/vendor/lib64/chipsetsdk/libffrt_acc.so", RTLD_LAZY | RTLD_NODELETE);
#else
    g_ffts_handle = dlopen("libffrt_acc.so", RTLD_LAZY | RTLD_NODELETE);
#endif
    if (g_ffts_handle != nullptr) {
#ifdef FFRT_PARSE_XML_USE_STUB
        ffrt::ParseXml::Instance().SetParaXmlPath("/vender/etc/ffrtqos_config_hw.xml");
#endif
        hffrt_init_func init_func = (hffrt_init_func)dlsym(g_ffts_handle, "DevFFTSInit");
        if (init_func != nullptr) {
            int ret = init_func();
            if (ret == 0) {
                g_ffrt_hw_property |= FFRT_HW_DYNAMIC_XPU_NORMAL;
                return;
            }
            dlclose(g_ffts_handle);
            g_ffts_handle = nullptr;
            FFRT_LOGW("Failed to init, ret: %d", ret);
        } else {
            dlclose(g_ffts_handle);
            g_ffts_handle = nullptr;
            FFRT_LOGW("Failed to get init func: %s", dlerror());
        }
    } else {
        FFRT_LOGW("Failed to load the shared library: %s", dlerror());
    }

    SFFRTInit();
}

__attribute__((destructor)) static void ffrt_deinit(void)
{
    if (g_ffts_handle != nullptr) {
        hffrt_deinit_func deinit_func = (hffrt_deinit_func)dlsym(g_ffts_handle, "DevFFTSDeinit");
        if (deinit_func != nullptr) {
            deinit_func();
        }
        dlclose(g_ffts_handle);
        g_ffts_handle = nullptr;
    }
}

void ChildHandle()
{
    ffrt_deinit();
    ffrt_init();
}

#ifdef __cplusplus
}
#endif