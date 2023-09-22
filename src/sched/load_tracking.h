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

#ifndef FFRT_LOAD_TRACKING_H
#define FFRT_LOAD_TRACKING_H

#include <list>
#include <vector>
#include <thread>
#include <chrono>
#include <unordered_map>

namespace ffrt {
struct TaskCtx;
class DefaultInterval;

enum class TaskSwitchState {
    BEGIN,
    UPDATE,
    END,
};

struct TaskSwitchRecord {
    uint64_t load;
    TaskSwitchState state;
    std::chrono::time_point<std::chrono::steady_clock> tp;
};

class UserSpaceLoadRecord {
public:
    void SetEnable(bool isEnable)
    {
        this->enable = isEnable;
    }

    bool Enable() const
    {
        return enable;
    }

    static void UpdateTaskSwitch(TaskCtx* prev, TaskCtx* next);

private:
    bool enable = false;
};

template <typename T>
class LoadTracking {
public:
    LoadTracking(DefaultInterval& it) : it(it)
    {
    }

    virtual ~LoadTracking() = default;

    void Begin()
    {
        static_cast<T*>(this)->BeginImpl();
    }

    void End()
    {
        static_cast<T*>(this)->EndImpl();
    }

    void Record(TaskSwitchState state)
    {
        static_cast<T*>(this)->RecordImpl(state);
    }

    uint64_t GetLoad()
    {
        return static_cast<T*>(this)->GetLoadImpl();
    }

protected:
    DefaultInterval& it;
};

class KernelLoadTracking : public LoadTracking<KernelLoadTracking> {
    friend class LoadTracking<KernelLoadTracking>;

public:
    KernelLoadTracking(DefaultInterval& it) : LoadTracking<KernelLoadTracking>(it)
    {
    }

private:
    void BeginImpl();
    void EndImpl();
    void RecordImpl(TaskSwitchState state)
    {
        (void)state;
    };
    uint64_t GetLoadImpl();
};

class UserSpaceLoadTracking : public LoadTracking<UserSpaceLoadTracking> {
    friend class LoadTracking<UserSpaceLoadTracking>;
    struct HistPoint;
    using RecordList = typename std::list<TaskSwitchRecord>;

public:
    UserSpaceLoadTracking(DefaultInterval& it);

private:
    void BeginImpl();
    void EndImpl();
    void RecordImpl(TaskSwitchState state);
    uint64_t GetLoadImpl();

    void RecordSwitchPoint(TaskSwitchState state, bool force = false);

    std::vector<HistPoint> CollectHistList();

    std::unordered_map<std::thread::id, RecordList> records;
};
}; // namespace ffrt

#endif
