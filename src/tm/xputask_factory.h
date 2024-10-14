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
#ifndef _XPU_TASK_FACTORY_H_
#define _XPU_TASK_FACTORY_H_

#include "xpueu_task.h"
#include "util/cb_func.h"

namespace ffrt {

class XPUTaskFactory {
public:
    static XPUTaskFactory &Instance();

    static XPUEUTask *Alloc()
    {
        return Instance().alloc_();
    }

    static void Free(XPUEUTask *task)
    {
        Instance().free_(task);
    }

    static void RegistCb(TaskAllocCB<XPUEUTask>::Alloc &&alloc, TaskAllocCB<XPUEUTask>::Free &&free)
    {
        Instance().alloc_ = std::move(alloc);
        Instance().free_ = std::move(free);
    }

private:
    TaskAllocCB<XPUEUTask>::Alloc alloc_;
    TaskAllocCB<XPUEUTask>::Free free_;
};

} // namespace ffrt
#endif