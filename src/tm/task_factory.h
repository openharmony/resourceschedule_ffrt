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

#ifndef TASK_FACTORY_HPP
#define TASK_FACTORY_HPP

#include "tm/cpu_task.h"
#include "util/cb_func.h"

namespace ffrt {

class TaskFactory {
public:
    static TaskFactory &Instance();

    static CPUEUTask *Alloc()
    {
        return Instance().alloc_();
    }

    static void Free(CPUEUTask *task)
    {
        Instance().free_(task);
    }

    static std::vector<void *> GetUnfreedMem()
    {
        if (Instance().getUnfreedMem_ != nullptr) {
            return Instance().getUnfreedMem_();
        }
        return {};
    }

    static void RegistCb(TaskAllocCB<CPUEUTask>::Alloc &&alloc, TaskAllocCB<CPUEUTask>::Free &&free,
        TaskAllocCB<CPUEUTask>::GetUnfreedMem &&getUnfreedMem = nullptr)
    {
        Instance().alloc_ = std::move(alloc);
        Instance().free_ = std::move(free);
        Instance().getUnfreedMem_ = std::move(getUnfreedMem);
    }

private:
    TaskAllocCB<CPUEUTask>::Free free_;
    TaskAllocCB<CPUEUTask>::Alloc alloc_;
    TaskAllocCB<CPUEUTask>::GetUnfreedMem getUnfreedMem_;
};

} // namespace ffrt

#endif