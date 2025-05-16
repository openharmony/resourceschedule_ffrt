/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#ifdef FFRT_ENABLE_HITRACE_CHAIN

#include "ffrt_trace_chain.h"
#include <dlfcn.h>

#ifdef APP_USE_ARM
constexpr const char* TRACE_CHAIN_LIB_PATH = "/system/lib/chipset-pub-sdk/libhitracechain.so";
#else
constexpr const char* TRACE_CHAIN_LIB_PATH = "/system/lib64/chipset-pub-sdk/libhitracechain.so";
#endif

namespace ffrt {

TraceChainAdapter::TraceChainAdapter()
{
    Load();
}

TraceChainAdapter::~TraceChainAdapter()
{
    UnLoad();
}

TraceChainAdapter& TraceChainAdapter::Instance()
{
    static TraceChainAdapter instance;
    return instance;
}

HiTraceIdStruct TraceChainAdapter::HiTraceChainGetId()
{
    if (getIdFunc_ != nullptr) {
        return getIdFunc_();
    }
    return {};
}

void TraceChainAdapter::HiTraceChainClearId()
{
    if (clearIdFunc_ != nullptr) {
        clearIdFunc_();
    }
}

void TraceChainAdapter::HiTraceChainRestoreId(const HiTraceIdStruct* pId)
{
    if (restoreIdFunc_ != nullptr) {
        restoreIdFunc_(pId);
    }
}

HiTraceIdStruct TraceChainAdapter::HiTraceChainCreateSpan()
{
    if (createSpanFunc_ != nullptr) {
        return createSpanFunc_();
    }
    return {};
}

void TraceChainAdapter::Load()
{
    if (handle_ != nullptr) {
        FFRT_LOGD("handle exits");
        return;
    }

    handle_ = dlopen(TRACE_CHAIN_LIB_PATH, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
    if (handle_ == nullptr) {
        FFRT_LOGE("load so[%s] fail", TRACE_CHAIN_LIB_PATH);
        return;
    }

    getIdFunc_ = reinterpret_cast<HiTraceChainGetIdFunc>(dlsym(handle_, "HiTraceChainGetId"));
    if (getIdFunc_ == nullptr) {
        FFRT_LOGE("load func HiTraceChainGetId from %s failed", TRACE_CHAIN_LIB_PATH);
        UnLoad();
        return;
    }

    clearIdFunc_ = reinterpret_cast<HiTraceChainClearIdFunc>(dlsym(handle_, "HiTraceChainClearId"));
    if (clearIdFunc_ == nullptr) {
        FFRT_LOGE("load func HiTraceChainClearId from %s failed", TRACE_CHAIN_LIB_PATH);
        UnLoad();
        return;
    }

    restoreIdFunc_ = reinterpret_cast<HiTraceChainRestoreIdFunc>(dlsym(handle_, "HiTraceChainRestoreId"));
    if (restoreIdFunc_ == nullptr) {
        FFRT_LOGE("load func HiTraceChainRestoreId from %s failed", TRACE_CHAIN_LIB_PATH);
        UnLoad();
        return;
    }

    createSpanFunc_ = reinterpret_cast<HiTraceChainCreateSpanFunc>(dlsym(handle_, "HiTraceChainCreateSpan"));
    if (createSpanFunc_ == nullptr) {
        FFRT_LOGE("load func HiTraceChainCreateSpan from %s failed", TRACE_CHAIN_LIB_PATH);
        UnLoad();
        return;
    }
}

void TraceChainAdapter::UnLoad()
{
    getIdFunc_ = nullptr;
    clearIdFunc_ = nullptr;
    restoreIdFunc_ = nullptr;
    createSpanFunc_ = nullptr;
    if (handle_ != nullptr) {
        if (dlclose(handle_) == 0) {
            handle_ = nullptr;
        }
    }
}

} // namespace ffrt

#endif // FFRT_ENABLE_HITRACE_CHAIN