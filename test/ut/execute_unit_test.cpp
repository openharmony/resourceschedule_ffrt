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
#include "eu/execute_unit.h"
#include "eu/thread_group.h"
#include "internal_inc/types.h"

using namespace testing;
using namespace testing::ext;
using namespace ffrt;
using namespace std;


class ExecuteUnitTest : public testing::Test {
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
 * @tc.name: NotifyTaskAdded
 * @tc.desc: Test whether the NotifyTaskAdded interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, NotifyTaskAdded, TestSize.Level1)
{
    ExecuteUnit::Instance().NotifyTaskAdded(QoS(qos(5)));
}

/**
 * @tc.name: BindWG
 * @tc.desc: Test whether the BindWG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, BindWG, TestSize.Level1)
{
    QoS *qos1 = new QoS();
    ExecuteUnit::Instance().BindWG(DevType(0), *qos1);
}

/**
 * @tc.name: UnbindTG
 * @tc.desc: Test whether the UnbindTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, UnbindTG, TestSize.Level1)
{
    QoS *qos1 = new QoS();
    ExecuteUnit::Instance().UnbindTG(DevType(0), *qos1);
}

/**
 * @tc.name: BindTG
 * @tc.desc: Test whether the BindTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, BindTG, TestSize.Level1)
{
    QoS *qos1 = new QoS();
    ThreadGroup* it = ExecuteUnit::Instance().BindTG(DevType(0), *qos1);
}
