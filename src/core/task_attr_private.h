/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef TASK_ATTR_PRIVATE_H
#define TASK_ATTR_PRIVATE_H
#include <string>
#include "c/type_def_ext.h"
#include "cpp/task_ext.h"
#include "qos.h"

namespace ffrt {
class task_attr_private {
public:
    task_attr_private()
        : qos_(qos_default)
    {
    }

    explicit task_attr_private(const task_attr attr)
        : qos_(attr.qos()),
          name_(attr.name()),
          delay_(attr.delay()),
          prio_(attr.priority())
    {
    }

    int qos_;
    std::string name_;
    uint64_t delay_ = 0;
    uint64_t timeout_ = 0;
    ffrt_queue_priority_t prio_ = ffrt_queue_priority_low;
    bool taskLocal_ = false;
    ffrt_function_header_t* timeoutCb_ = nullptr;
};
}
#endif
