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

#ifndef FFRT_TASK_STATE_HPP
#define FFRT_TASK_STATE_HPP

#include <array>
#include <string_view>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <chrono>

namespace ffrt {
class CPUEUTask;

class TaskState {
public:
    enum State { PENDING, READY, RUNNING, BLOCKED, EXITED, MAX };

    using Op = typename std::function<bool(CPUEUTask*)>;

    TaskState() = default;

    TaskState(const TaskState&) = delete;
    TaskState(TaskState&&) = delete;

    TaskState& operator=(const TaskState&) = delete;
    TaskState& operator=(TaskState&&) = delete;

    bool operator==(State state) const
    {
        return this->curState == state;
    }

    bool operator!=(State state) const
    {
        return this->curState != state;
    }

    State operator()() const
    {
        return curState;
    }

    State CurState() const
    {
        return curState;
    }

#ifdef FFRT_IO_TASK_SCHEDULER
    void SetCurState(State state)
    {
        curState = state;
    }
#endif

    State PreState() const
    {
        return preState;
    }

    const char* String() const
    {
        return String(curState);
    }

    uint64_t WaitingTime() const
    {
#if defined(TRACE_TASKSTAT_LOG_ENABLE) && (TRACE_TASKSTAT_LOG_ENABLE == 1)
        return stat.WaitingTime();
#else
        return 0;
#endif
    }

    uint64_t RunningTime() const
    {
#if defined(TRACE_TASKSTAT_LOG_ENABLE) && (TRACE_TASKSTAT_LOG_ENABLE == 1)
        return stat.RunningTime();
#else
        return 0;
#endif
    }

    static void RegisterOps(State state, Op&& op)
    {
        ops[static_cast<size_t>(state)] = op;
    }

    static int OnTransition(State state, CPUEUTask* task, Op&& op = Op());

    static const char* String(State state)
    {
        static const char* m[] = {"PENDING", "READY", "RUNNING", "BLOCKED", "EXITED", "MAX"};

        return m[static_cast<size_t>(state)];
    }

private:
    class TaskStateStat {
    public:
        uint64_t WaitingTime() const;
        uint64_t RunningTime() const;

        inline void Count(CPUEUTask* task);

    private:
        inline void Count(TaskState::State state);
        inline uint64_t CalcDuration(TaskState::State pre, TaskState::State cur) const;

        std::array<std::chrono::steady_clock::time_point, static_cast<size_t>(TaskState::State::MAX)> timepoint;
    };

#if defined(TRACE_TASKSTAT_LOG_ENABLE) && (TRACE_TASKSTAT_LOG_ENABLE == 1)
    TaskStateStat stat;
#endif

    State curState = PENDING;
    State preState = PENDING;

    static std::array<Op, static_cast<size_t>(TaskState::MAX)> ops;
};
} // namespace ffrt

#endif
