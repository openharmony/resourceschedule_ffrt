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
#ifndef FFRT_INTERFACE_HANDLER_H
#define FFRT_INTERFACE_HANDLER_H

#include <cstdint>
#include "internal_inc/non_copyable.h"

namespace ffrt {
class ITask;
class IHandler : private NonCopyable {
public:
    virtual ~IHandler() = default;
    virtual int Cancel(ITask* task) = 0;
    virtual void DispatchTask(ITask* task) = 0;
    virtual int SubmitDelayed(ITask* task, uint64_t delayUs) = 0;
};
} // namespace ffrt

#endif // FFRT_INTERFACE_HANDLER_H