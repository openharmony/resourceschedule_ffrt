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

    void insert(std::string name, ffrt_executor_task_func func)
    {
        func_map[name] = func;
    }

    ffrt_executor_task_func getFunc(std::string name)
    {
        if (func_map.find(name) == func_map.end()) {
            return nullptr;
        }
        return func_map[name];
    }

private:
    FuncManager()
    {
    }
    std::map<std::string, ffrt_executor_task_func> func_map;
};
}
#endif /* FFRT_FUNC_MANAGER_HPP */
