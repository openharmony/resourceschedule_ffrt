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
#include "util/ffrt_facade.h"
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


class ExecuteUnitTest : public testing::Test {
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
 * @tc.name: NotifyTaskAdded
 * @tc.desc: Test whether the NotifyTaskAdded interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, NotifyTaskAdded, TestSize.Level1)
{
    FFRTFacade::GetEUInstance().NotifyTaskAdded(QoS(qos(5)));
}

/**
 * @tc.name: BindWG
 * @tc.desc: Test whether the BindWG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, BindWG, TestSize.Level1)
{
    QoS *qos1 = new QoS();
    FFRTFacade::GetEUInstance().BindWG(DevType(0), *qos1);
    EXPECT_EQ(*qos1, qos_default);
}

/**
 * @tc.name: UnbindTG
 * @tc.desc: Test whether the UnbindTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, UnbindTG, TestSize.Level1)
{
    QoS *qos1 = new QoS();
    FFRTFacade::GetEUInstance().UnbindTG(DevType(0), *qos1);
    EXPECT_EQ(*qos1, qos_default);
}

/**
 * @tc.name: BindTG
 * @tc.desc: Test whether the BindTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, BindTG, TestSize.Level1)
{
    QoS *qos1 = new QoS();
    ThreadGroup* it = FFRTFacade::GetEUInstance().BindTG(DevType(0), *qos1);
    EXPECT_EQ(*qos1, qos_default);
}

}
}
