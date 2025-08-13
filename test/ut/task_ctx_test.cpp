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
#include "dm/dependence_manager.h"
#ifdef USE_OHOS_QOS
#include "qos.h"
#else
#include "staging_qos/sched/qos.h"
#endif
#include "ffrt_inner.h"
#include "internal_inc/types.h"
#include "common.h"

namespace OHOS {
namespace FFRT_TEST {
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace OHOS::FFRT_TEST;
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

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

/**
 * @tc.name: ChargeQoSSubmit
 * @tc.desc: Test whether the ChargeQoSSubmit interface are normal.
 * @tc.type: FUNC
 */

HWTEST_F(TaskCtxTest, ChargeQoSSubmit, TestSize.Level0)
{
    auto func = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    auto task = std::make_unique<SCPUEUTask>(nullptr, nullptr, 0);
    QoS qos = QoS(static_cast<int>(qos_inherit));
    task->SetQos(qos);
    EXPECT_EQ(task->qos_, qos_default);

    auto func1 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    auto task1 = std::make_unique<SCPUEUTask>(nullptr, nullptr, 0);
    auto func2 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    auto task2 = std::make_unique<SCPUEUTask>(nullptr, task1.get(), 0);
    QoS qos2 = QoS(static_cast<int>(qos_inherit));
    task2->SetQos(qos2);
    EXPECT_EQ(task2->qos_, static_cast<int>(qos_default));

    auto func3 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    auto task3 = std::make_unique<SCPUEUTask>(nullptr, nullptr, 0);
    QoS qos3 = QoS(static_cast<int>(qos_user_interactive));
    task3->SetQos(qos3);
    EXPECT_EQ(task3->qos_, static_cast<int>(qos_user_interactive));

}
}
}
