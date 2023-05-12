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

#include "sched/load_tracking.h"

#include <unordered_set>
#include <unordered_map>

#include <unistd.h>

#include "core/entity.h"
#include "core/dependence_manager.h"
#include "sched/interval.h"

namespace ffrt {
#define perf_mmap_read_current() (static_cast<uint64_t>(0))
static UserSpaceLoadRecord uRecord;

void UserSpaceLoadRecord::UpdateTaskSwitch(TaskCtx* prev, TaskCtx* next)
{
    if (!uRecord.Enable() || (!prev && !next)) {
        return;
    }

    auto getIntersect = [](const std::unordered_set<Interval*>& thisSet,
                            const std::unordered_set<Interval*>& otherSet) {
        std::unordered_set<Interval*> set;
        for (auto it : thisSet) {
            if (otherSet.find(it) != otherSet.end()) {
                set.insert(it);
            }
        }
        return set;
    };

    auto updateLt = [](const std::unordered_set<Interval*>& set, const std::unordered_set<Interval*>& intersetSet,
                        TaskSwitchState state) {
        for (auto it : set) {
            if (intersetSet.find(it) != intersetSet.end()) {
                continue;
            }
            it->UpdateTaskSwitch(state);
        }
    };

    std::unordered_set<Interval*> intersectSet;
    if (prev && next) {
        intersectSet = getIntersect(prev->relatedIntervals, next->relatedIntervals);
        for (auto it : intersectSet) {
            it->UpdateTaskSwitch(TaskSwitchState::UPDATE);
        }
    }

    if (prev) {
        updateLt(prev->relatedIntervals, intersectSet, TaskSwitchState::END);
    }

    if (next) {
        updateLt(next->relatedIntervals, intersectSet, TaskSwitchState::BEGIN);
    }
}

void KernelLoadTracking::BeginImpl()
{
    it.Ctrl().Begin();
}

void KernelLoadTracking::EndImpl()
{
    it.Ctrl().End();
}

uint64_t KernelLoadTracking::GetLoadImpl()
{
    return it.Ctrl().GetLoad();
}

struct UserSpaceLoadTracking::HistPoint {
    TaskSwitchRecord* record;
    TaskSwitchState state;
    double load;
    std::chrono::time_point<std::chrono::steady_clock> tp;
};

UserSpaceLoadTracking::UserSpaceLoadTracking(DefaultInterval& it) : LoadTracking<KernelLoadTracking>(it)
{
    uRecord.SetEnable(true);
}

void UserSpaceLoadTracking::BeginImpl()
{
    auto ctx = ExecuteCtx::Cur();
    auto task = ctx->task ? ctx->task : DependenceManager::Root();
    if (task->IsRoot() || it.Qos() == task->qos) {
        task->relatedIntervals.insert(&it);
    }
}

void UserSpaceLoadTracking::EndImpl()
{
    DependenceManager::Root()->relatedIntervals.erase(&it);
    records.clear();
}

void UserSpaceLoadTracking::RecordImpl(TaskSwitchState state)
{
    RecordSwitchPoint(state);
}

uint64_t UserSpaceLoadTracking::GetLoadImpl()
{
    auto histList = CollectHistList();

    double totalLoad = 0;
    std::unordered_map<TaskSwitchRecord*, double> filter;

    auto updateTotalLoad = [&](size_t i) {
        if (filter.size() == 0) {
            return;
        }

        double delta = (histList[i].tp - histList[i - 1].tp).count();
        if (delta <= 0) {
            return;
        }

        double maxLps = 0;
        for (const auto& f : filter) {
            maxLps = std::max(maxLps, f.second);
        }

        totalLoad += maxLps * delta;
    };

    for (size_t i = 0; i < histList.size(); ++i) {
        updateTotalLoad(i);

        switch (histList[i].state) {
            case TaskSwitchState::BEGIN:
                filter[histList[i].record] = histList[i].load;
                break;
            default:
                filter.erase(histList[i].record);
                break;
        }
    }

    return static_cast<uint64_t>(totalLoad);
}

void UserSpaceLoadTracking::RecordSwitchPoint(TaskSwitchState state, bool force)
{
    auto& record = records[std::this_thread::get_id()];
    auto tp = std::chrono::steady_clock::now();
    if (state == TaskSwitchState::UPDATE && !force && !record.empty() &&
        tp - record.back().tp < std::chrono::milliseconds(1)) {
        return;
    }

    record.emplace_back(TaskSwitchRecord {perf_mmap_read_current(), state, tp});
}

std::vector<UserSpaceLoadTracking::HistPoint> UserSpaceLoadTracking::CollectHistList()
{
    std::vector<HistPoint> histList;

    auto collectHist = [&histList](RecordList::iterator& it, size_t index, size_t size) {
        auto& cur = *it;

        // deal task begin
        if (cur.state != TaskSwitchState::END && index + 1 < size) {
            const auto& next = *++it;
            double load = (next.load - cur.load) / (next.tp - cur.tp).count();
            histList.emplace_back(HistPoint {&cur, TaskSwitchState::BEGIN, load, cur.tp});
            --it;
        }

        // deal task end
        if (cur.state != TaskSwitchState::BEGIN && index > 0) {
            auto& prev = *--it;
            histList.emplace_back(HistPoint {&prev, TaskSwitchState::END, 0, cur.tp});
            ++it;
        }
    };

    RecordSwitchPoint(TaskSwitchState::UPDATE, true);

    for (auto& record : records) {
        auto& list = record.second;

        size_t index = 0;
        size_t size = list.size();
        for (auto it = list.begin(); it != list.end(); ++it, ++index) {
            collectHist(it, index, size);
        }
    }

    std::sort(histList.begin(), histList.end(),
        [](const HistPoint& x, const HistPoint& y) { return x.tp < y.tp || x.state > y.state; });

    return histList;
}

}; // namespace ffrt
