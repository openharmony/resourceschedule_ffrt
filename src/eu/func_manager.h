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

#ifndef FFRT_FUNC_MANAGER_HPP
#define FFRT_FUNC_MANAGER_HPP

#include "ffrt.h"

namespace ffrt {
class FuncManager {
public:
    FuncManager(const FuncManager&) = delete;
    FuncManager& operator=(const FuncManager&) = delete;
    ~FuncManager()
    {
    }

    // 获取FuncManager的单例
    static inline FuncManager* Instance()
    {
        static FuncManager func_mg;
        return &func_mg;
    }

    void insert(ffrt_executor_task_type_t task_type, ffrt_executor_task_func func)
    {
        func_map[task_type] = func;
    }

    ffrt_executor_task_func getFunc(ffrt_executor_task_type_t task_type)
    {
        if (func_map.find(task_type) == func_map.end()) {
            return nullptr;
        }
        return func_map[task_type];
    }

private:
    FuncManager()
    {
        func_map[ffrt_io_task] = nullptr;
        func_map[ffrt_uv_task] = nullptr;
    }
    std::unordered_map<ffrt_executor_task_type_t, ffrt_executor_task_func> func_map;
};
}
#endif /* FFRT_FUNC_MANAGER_HPP */
