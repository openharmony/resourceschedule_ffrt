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

#ifndef __FFRT_FLO_OHOS_H__
#define __FFRT_FLO_OHOS_H__
#include <string>
#include <dlfcn.h>
#include "dfx/log/ffrt_log_api.h"

namespace ffrt {
int cpu_boost_start(int ctx_id);
int cpu_boost_end(int ctx_id);
int cpu_boost_save(int ctx_id);
int cpu_boost_restore(int ctx_id);
const std::string FLO_LIB_PATH = "lib_cpuboost.so";
class FloAdapter {
public:
    FloAdapter()
    {
        Load();
    }

    ~FloAdapter()
    {
        UnLoad();
    }

    static FloAdapter* Instance()
    {
        static FloAdapter instance;
        return &instance;
    }

#define REG_FUNC(func) using func##Type = decltype(func)*; func##Type func##Temp = nullptr
    REG_FUNC(cpu_boost_start);
    REG_FUNC(cpu_boost_end);
    REG_FUNC(cpu_boost_save);
    REG_FUNC(cpu_boost_restore);
#undef REG_FUNC

private:
    void* handle = nullptr;
    bool Load()
    {
        if (handle != nullptr) {
            FFRT_LOGD("handle exist");
            return true;
        }

        handle = dlopen(FLO_LIB_PATH.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            FFRT_LOGE("load sd[%s] fail", FLO_LIB_PATH.c_str());
            return false;
        }

#define LOAD_FUNC(x) x##Temp = reinterpret_cast<x##Type>(dlsym(handle, #x)); \
        if (x##Temp == nullptr) \
        { \
            FFRT_LOGE("load func %s from %s fail", #x, FLO_LIB_PATH.c_str()); \
            return false; \
        }
            LOAD_FUNC(cpu_boost_start);
            LOAD_FUNC(cpu_boost_end);
            LOAD_FUNC(cpu_boost_save);
            LOAD_FUNC(cpu_boost_restore);
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


};

#define EXECUTE_FLO_FUNC(x, ctx_id, ret) auto func = FloAdapter::Instance()->x##Temp; \
        if (func != nullptr) { \
            ret = func(ctx_id); \
        } else {
            ret = -1; \
        }

inline int FloStart(int ctx_id)
{
    int ret = 0;
    EXECUTE_FLO_FUNC(cpu_boost_start, ctx_id, ret);
    return ret;
}

inline int FloEnd(int ctx_id)
{
    int ret = 0;
    EXECUTE_FLO_FUNC(cpu_boost_end, ctx_id, ret);
    return ret;
}

inline int FloSave(int ctx_id)
{
    int ret = 0;
    EXECUTE_FLO_FUNC(cpu_boost_save, ctx_id, ret);
    return ret;
}

inline int FloRestore(int ctx_id)
{
    int ret = 0;
    EXECUTE_FLO_FUNC(cpu_boost_restore, ctx_id, ret);
    return ret;
}
}
#endif