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

#include "sched/task_manager.h"

#include "dfx/log/ffrt_log_api.h"

using namespace ffrt;

void TaskManager::TaskStateCount(CPUEUTask* task)
{
    if (task->state.PreState() == task->state.CurState()) {
        return;
    }

    DecreCount(taskCount[static_cast<size_t>(task->state.PreState())]);
    IncreCount(taskCount[static_cast<size_t>(task->state.CurState())]);

    CalcTaskTime(task);
}

void TaskManager::CalcTaskTime(CPUEUTask* task)
{
    auto calc = [](uint64_t time, uint64_t count, std::atomic_uint64_t& maxTime, std::atomic<double>& avgTime) {
        if (time > maxTime) {
            maxTime = time;
        }

        if (count <= 1) {
            avgTime = time;
        } else {
            avgTime = avgTime + (time - avgTime) / count;
        }
    };

    switch (task->state.CurState()) {
        case TaskState::RUNNING:
            calc(task->state.WaitingTime(), GetCount(TaskState::READY), maxWaitingTime, avgWaitingTime);
            break;
        case TaskState::EXITED: {
            uint64_t count = GetCount(TaskState::EXITED);
            calc(task->state.RunningTime(), count, maxRunningTime, avgRunningTime);
            if ((count & 0x3FF) == 0) {
                FFRT_LOGW(
                    "Task-done num: %lu; wait-time aver : %.2f us, max: %lu us; exec-time: aver %.2f us, max: %lu us",
                    count, avgWaitingTime.load(), maxWaitingTime.load(), avgRunningTime.load(), maxRunningTime.load());
            }
            break;
        }
        default:
            break;
    }
}