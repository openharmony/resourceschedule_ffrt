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

#include "sched/task_state.h"
#include "sched/task_manager.h"
#include "dfx/log/ffrt_log_api.h"
#include "sched/scheduler.h"

namespace ffrt {
std::array<TaskState::Op, static_cast<size_t>(TaskState::MAX)> TaskState::ops;

int TaskState::OnTransition(State state, CPUEUTask* task, Op&& op)
{
    if (task == nullptr) {
        FFRT_LOGE("task nullptr");
        return -1;
    }
    if (task->IsRoot()) {
        FFRT_LOGD("task root no state transition");
        return 0;
    }

    if (task->state == TaskState::EXITED) {
        FFRT_LOGE("task[%s] have finished", task->label.c_str());
        return -1;
    }

    task->state.preState = task->state.curState;
    task->state.curState = state;

    FFRT_LOGD(
        "task(%s) status: %s -=> %s ", task->label.c_str(), String(task->state.preState), String(task->state.curState));

#if (TASKSTAT_LOG_ENABLE == 1)
    task->state.stat.Count(task);
#endif

    if (ops[static_cast<size_t>(state)] &&
        !ops[static_cast<size_t>(state)](task)) {
        return -1;
    }

    if (op && !op(task)) {
        return -1;
    }

    return 0;
}

uint64_t TaskState::TaskStateStat::WaitingTime() const
{
    return CalcDuration(TaskState::READY, TaskState::RUNNING);
}

uint64_t TaskState::TaskStateStat::RunningTime() const
{
    return CalcDuration(TaskState::RUNNING, TaskState::EXITED);
}

void TaskState::TaskStateStat::Count(CPUEUTask* task)
{
    Count(task->state.CurState());
    TaskManager::Instance().TaskStateCount(task);
}

void TaskState::TaskStateStat::Count(State state)
{
    size_t index = static_cast<size_t>(state);
    switch (state) {
        case TaskState::READY:
            if (timepoint[index].time_since_epoch() == std::chrono::steady_clock::duration::zero()) {
                timepoint[index] = std::chrono::steady_clock::now();
            }
            break;
        case TaskState::MAX:
            break;
        default:
            timepoint[index] = std::chrono::steady_clock::now();
            break;
    }
}

uint64_t TaskState::TaskStateStat::CalcDuration(State pre, State cur) const
{
    return timepoint[static_cast<size_t>(cur)].time_since_epoch() == std::chrono::steady_clock::duration::zero() ?
        0 :
        std::chrono::duration_cast<std::chrono::microseconds>(
            timepoint[static_cast<size_t>(cur)] - timepoint[static_cast<size_t>(pre)])
            .count();
}
} // namespace ffrt
