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

#ifndef XPUEU_TASK_H
#define XPUEU_TASK_H
#include "tm/cpu_task.h"
#include "core/task_attr_private.h"
#include "c/ffrt_types.h"

namespace ffrt {
class XPUEUTask : public TaskBase {
public:
    XPUEUTask(const uint64_t &id, const QoS &qos)
        : TaskBase(ffrt_xpu_task, nullptr), rank(id), result(FFRT_HW_EXEC_SUCC)
    {
        label = "XPU_t" + std::to_string(rank);
        this->qos_ = qos;
    }
    ~XPUEUTask() override = default;

    void FreeMem() override;

    std::string GetLabel() const override
    {
        return label;
    }

    int GetResult() const
    {
        return result;
    }
    SkipStatus skipped = SkipStatus::SUBMITTED;
    const uint64_t rank;
    int result;
    std::string label;
    std::vector<CPUEUTask*> in_handles;
};
}
#endif