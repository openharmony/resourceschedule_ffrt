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

#include <list>
#include <vector>
#include <queue>
#include <thread>
#include <gtest/gtest.h>
#define private public
#define protect public
#include "ffrt_inner.h"

#include "core/entity.h"
#include "sched/task_scheduler.h"
#include "sched/task_manager.h"
#include "core/task_attr_private.h"
#include "tm/scpu_task.h"
#include "sched/scheduler.h"
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class SchedulerTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

HWTEST_F(SchedulerTest, taskstate_test, TestSize.Level1)
{
    std::queue<std::unique_ptr<SCPUEUTask>> tasks;

    std::vector<TaskState::State> produceStatus;
    std::vector<TaskState::State> consumeStatus;

#if (TASKSTAT_LOG_ENABLE == 1)
    std::array<uint64_t, static_cast<size_t>(TaskState::MAX)> expectCount;

    // record previous test units count
    for (auto state = TaskState::PENDING; state != TaskState::MAX; ++(size_t&)state) {
        expectCount[static_cast<size_t>(state)] = TaskManager::Instance().GetCount(state);
    }

    // expect non-exited state count equal zero
    EXPECT_EQ(expectCount[static_cast<size_t>(TaskState::PENDING)], 0);
    EXPECT_EQ(expectCount[static_cast<size_t>(TaskState::READY)], 0);
    EXPECT_EQ(expectCount[static_cast<size_t>(TaskState::RUNNING)], 0);
    EXPECT_EQ(expectCount[static_cast<size_t>(TaskState::BLOCKED)], 0);

    auto increCount = [&expectCount](TaskState::State state) { ++expectCount[static_cast<size_t>(state)]; };

    auto decreCount = [&expectCount](TaskState::State state) {
        if (expectCount[static_cast<size_t>(state)] > 0) {
            --expectCount[static_cast<size_t>(state)];
        }
    };
#endif
    auto setState = [&](CPUEUTask* task) {
        consumeStatus.emplace_back(task->state());
        return true;
    };

    auto getNextState = [](TaskState::State state) {
        switch (state) {
            case TaskState::PENDING:
                return TaskState::READY;
            case TaskState::READY:
                return TaskState::RUNNING;
            case TaskState::RUNNING:
                return TaskState::BLOCKED;
            case TaskState::BLOCKED:
                return TaskState::EXITED;
            default:
                break;
        }
        return TaskState::MAX;
    };

    TaskState::RegisterOps(TaskState::READY, setState);
    TaskState::RegisterOps(TaskState::RUNNING, setState);
    TaskState::RegisterOps(TaskState::BLOCKED, setState);
    TaskState::RegisterOps(TaskState::EXITED, setState);

    task_attr_private task_attr;
    task_attr.name_ = "root";
    auto root = std::make_unique<SCPUEUTask>(
        &task_attr, nullptr, 0);
    for (int i = 1; i <= 1000; ++i) {
        task_attr_private task_attr;
        task_attr.name_ = "i";
        tasks.push(std::make_unique<SCPUEUTask>(
            &task_attr, root.get(), i));
    }

    while (!tasks.empty()) {
        auto task = std::move(tasks.front());
        tasks.pop();

        auto state = getNextState(task->state());
        if (state == TaskState::MAX) {
            continue;
        }

        produceStatus.emplace_back(state);

        task->UpdateState(state);

#if (TASKSTAT_LOG_ENABLE == 1)
        auto preState = task->state.PreState();
        auto curState = task->state.CurState();

        decreCount(preState);
        increCount(curState);

        EXPECT_EQ(expectCount[static_cast<size_t>(preState)], TaskManager::Instance().GetCount(preState));
        EXPECT_EQ(expectCount[static_cast<size_t>(curState)], TaskManager::Instance().GetCount(curState));
#endif

        tasks.push(std::move(task));
    }

#if (TRACE_TASKSTAT_LOG_ENABLE == 1)
    EXPECT_EQ(
        expectCount[static_cast<size_t>(TaskState::PENDING)], TaskManager::Instance().GetCount(TaskState::PENDING));
    EXPECT_EQ(expectCount[static_cast<size_t>(TaskState::READY)], TaskManager::Instance().GetCount(TaskState::READY));
    EXPECT_EQ(
        expectCount[static_cast<size_t>(TaskState::RUNNING)], TaskManager::Instance().GetCount(TaskState::RUNNING));
    EXPECT_EQ(
        expectCount[static_cast<size_t>(TaskState::BLOCKED)], TaskManager::Instance().GetCount(TaskState::BLOCKED));
    EXPECT_EQ(expectCount[static_cast<size_t>(TaskState::EXITED)], TaskManager::Instance().GetCount(TaskState::EXITED));
#endif

    EXPECT_EQ(produceStatus.size(), consumeStatus.size());

    int size = produceStatus.size();
    for (int i = 0; i < size; ++i) {
        EXPECT_EQ(produceStatus[i], consumeStatus[i]);
    }
}

HWTEST_F(SchedulerTest, taskstateCount_test, TestSize.Level1)
{
    SCPUEUTask* task1 = new SCPUEUTask(nullptr, nullptr, 0, QoS(static_cast<int>(qos_user_interactive)));
    SCPUEUTask *task2 = new SCPUEUTask(nullptr, task1, 0, QoS());
    EXPECT_NE(task2, nullptr);
    TaskManager::Instance().TaskStateCount(task2);
    delete task1;
    delete task2;
}

HWTEST_F(SchedulerTest, ffrt_task_runqueue_test, TestSize.Level1)
{
    ffrt::FIFOQueue *fifoqueue = new ffrt::FIFOQueue();
    int aimnum = 10;
    SCPUEUTask task(nullptr, nullptr, 0, QoS(static_cast<int>(qos_user_interactive)));
    for (int i = 0; i < aimnum ; i++) {
        fifoqueue->EnQueue(&task);
    }
    EXPECT_EQ(fifoqueue->Size(), aimnum);
    EXPECT_EQ(fifoqueue->Empty(), false);
    delete fifoqueue;
}

HWTEST_F(SchedulerTest, ffrt_scheduler_test, TestSize.Level1)
{
    ffrt::FFRTScheduler* sffrtscheduler = ffrt::FFRTScheduler::Instance();
    QoS qos;
    ffrt::IOTaskExecutor* task = new (std::nothrow) ffrt::IOTaskExecutor(qos);
    LinkedList* node = reinterpret_cast<LinkedList *>(&task->wq);
    EXPECT_EQ(sffrtscheduler->InsertNode(reinterpret_cast<LinkedList*>(node), qos), true);
    EXPECT_EQ(sffrtscheduler->RemoveNode(reinterpret_cast<LinkedList*>(node), qos), true);

    delete task;
}

HWTEST_F(SchedulerTest, set_cur_state_test, TestSize.Level1)
{
    SCPUEUTask* task1 = new SCPUEUTask(nullptr, nullptr, 0, QoS(static_cast<int>(qos_user_interactive)));
    SCPUEUTask *task2 = new SCPUEUTask(nullptr, task1, 0, QoS());
    EXPECT_NE(task2, nullptr);
    task2->state.SetCurState(ffrt::TaskState::RUNNING);
    TaskManager::Instance().TaskStateCount(task2);
    delete task2;
    delete task1;
}