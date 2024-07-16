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
#include <thread>
#include <vector>
#include "dm/dependence_manager.h"
#include "qos.h"
#include "ffrt_inner.h"
#include "internal_inc/types.h"

using namespace testing;
using namespace testing::ext;
using namespace ffrt;
using namespace std;

class TaskCtxTest : public testing::Test {
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

/**
 * @tc.name: ChargeQoSSubmit
 * @tc.desc: Test whether the ChargeQoSSubmit interface are normal.
 * @tc.type: FUNC
 */

HWTEST_F(TaskCtxTest, ChargeQoSSubmit, TestSize.Level1)
{
    auto func = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    SCPUEUTask *task = new SCPUEUTask(nullptr, nullptr, 0, QoS());
    QoS qos = QoS(static_cast<int>(qos_inherit));
    task->SetQos(qos);
    EXPECT_EQ(task->qos, qos_default);
    delete task;

    auto func1 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    SCPUEUTask *task1 = new SCPUEUTask(nullptr, nullptr, 0, QoS(static_cast<int>(qos_user_interactive)));
    auto func2 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    SCPUEUTask *task2 = new SCPUEUTask(nullptr, task1, 0, QoS());
    QoS qos2 = QoS(static_cast<int>(qos_inherit));
    task2->SetQos(qos2);
    EXPECT_EQ(task2->qos, static_cast<int>(qos_user_interactive));
    delete task1;
    delete task2;

    auto func3 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    SCPUEUTask *task3 = new SCPUEUTask(nullptr, nullptr, 0, QoS());
    QoS qos3 = QoS(static_cast<int>(qos_user_interactive));
    task3->SetQos(qos3);
    EXPECT_EQ(task3->qos, static_cast<int>(qos_user_interactive));
    delete task3;
}

/**
 * @tc.name: WaitFailWhenReuseHandle
 * @tc.desc: Test use ffrt wait when reuse the handle.
 * @tc.type: FUNC
 */
HWTEST_F(TaskCtxTest, WaitFailWhenReuseHandle, TestSize.Level1)
{
    std::vector<ffrt::dependence> deps;
    {
        auto h = ffrt::submit_h([] { printf("task0 done\n"); });
        printf("task0 handle: %p\n:", static_cast<void*>(h));
        deps.emplace_back(h);
    }
    usleep(1000);
    std::atomic_bool stop = false;
    auto h = ffrt::submit_h([&] {
        printf("task1 start\n");
        while (!stop);
        printf("task1 done\n");
        });
    ffrt::wait(deps);
    stop = true;
    ffrt::wait();
}
