

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

#ifndef UTIL_TASK_DELETER_HPP
#define UTIL_TASK_DELETER_HPP

#include <atomic>
#include "internal_inc/non_copyable.h"
#include "sched/execute_ctx.h"

namespace ffrt {
class TaskDeleter : private NonCopyable {
public:
    TaskDeleter() {};
    virtual ~TaskDeleter() {}
    virtual void FreeMem() = 0;

    inline void IncDeleteRef()
    {
        rc.fetch_add(1);
    }

    inline void DecDeleteRef()
    {
        auto v = rc.fetch_sub(1);
        if (v == 1) {
            FreeMem();
        }
    }
    std::atomic_uint32_t rc = 1;
};
} // namespace ffrt

#endif // UTIL_TASK_DELETER_HPP
