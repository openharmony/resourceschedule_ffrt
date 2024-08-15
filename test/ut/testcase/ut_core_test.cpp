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

#include <gtest/gtest.h>
#include <random>
#include <csignal>
#include <cstdlib>
#include "core/entity.h"
#include "core/version_ctx.h"
#include "ffrt_inner.h"
#include "sched/task_state.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/bbox/bbox.h"
#include "tm/cpu_task.h"

using namespace std;
using namespace testing;
using namespace testing::ext;
using namespace ffrt;

class CoreTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

HWTEST_F(CoreTest, core_test_success_01, TestSize.Level1)
{
    sync_io(0);
}

HWTEST_F(CoreTest, task_ctx_success_01, TestSize.Level1)
{
    auto func1 = ([]() {cout << endl << "push a task" << endl;});
    SCPUEUTask *task1 = new SCPUEUTask(nullptr, nullptr, 0, QoS(static_cast<int>(qos_user_interactive)));
    auto func2 = ([]() {cout << endl << "push a task" << endl;});
    SCPUEUTask *task2 = new SCPUEUTask(nullptr, task1, 0, QoS());
    QoS qos = QoS(static_cast<int>(qos_inherit));
    task2->SetQos(qos);
    EXPECT_EQ(task2->qos, static_cast<int>(qos_user_interactive));
    delete task1;
    delete task2;
}

HWTEST_F(CoreTest, ThreadWaitAndNotifyMode, TestSize.Level1)
{
    SCPUEUTask* task = new SCPUEUTask(nullptr, nullptr, 0, QoS());

    // when executing task is nullptr
    EXPECT_EQ(ThreadWaitMode(nullptr), true);

    // when executing task is root
    EXPECT_EQ(ThreadWaitMode(task), true);

    //when executing task in legacy mode
    SCPUEUTask* parent = new SCPUEUTask(nullptr, nullptr, 0, QoS());
    task->parent = parent;
    task->legacyCountNum = 1;
    EXPECT_EQ(ThreadWaitMode(task), true);

    // when task is valid and not legacy mode
    task->legacyCountNum = 0;
    EXPECT_EQ(ThreadWaitMode(task), false);

    // when block thread is false
    EXPECT_EQ(ThreadNotifyMode(task), false);

    // when block thread is true
    task->blockType = BlockType::BLOCK_THREAD;
    EXPECT_EQ(ThreadNotifyMode(task), true);

    delete parent;
    delete task;
}