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
#ifndef FFRT_INTERFACE_TASK_H
#define FFRT_INTERFACE_TASK_H

#include "c/type_def.h"
#include "internal_inc/non_copyable.h"

namespace ffrt {
class IHandler;
class ITask : private NonCopyable {
public:
    virtual ~ITask() = default;

    virtual ITask* SetQueHandler(IHandler* handler) = 0;
    virtual void Wait() = 0;
    virtual void Notify() = 0;
    virtual void IncDeleteRef() = 0;
    virtual void DecDeleteRef() = 0;

    IHandler* handler_ = nullptr;
    uint8_t func_storage[ffrt_auto_managed_function_storage_size];
};
} // namespace ffrt

#endif // FFRT_INTERFACE_TASK_H