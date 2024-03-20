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

#ifndef FFRT_TASK_MANAGER_H
#define FFRT_TASK_MANAGER_H

#include <array>
#include <atomic>

#include "tm/cpu_task.h"

namespace ffrt {
class TaskManager {
public:
    static TaskManager& Instance()
    {
        static TaskManager manager;
        return manager;
    }

    uint64_t GetCount(TaskState::State state) const
    {
        return taskCount[static_cast<size_t>(state)];
    }

    void TaskStateCount(CPUEUTask* task);

private:
    TaskManager() = default;

    void IncreCount(std::atomic_uint64_t& count)
    {
        count.fetch_add(1, std::memory_order_relaxed);
    }

    void DecreCount(std::atomic_uint64_t& count)
    {
        while (true) {
            auto old = count.load();
            if (old <= 0) {
                break;
            }

            if (count.compare_exchange_weak(old, old - 1, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    inline void CalcTaskTime(CPUEUTask* task);

    std::array<std::atomic_uint64_t, static_cast<size_t>(TaskState::MAX)> taskCount;

    std::atomic_uint64_t maxWaitingTime;
    std::atomic<double> avgWaitingTime;

    std::atomic_uint64_t maxRunningTime;
    std::atomic<double> avgRunningTime;
};
} // namespace ffrt

#endif