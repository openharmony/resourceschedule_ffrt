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
#include "cpp/deadline.h"
#include "internal_inc/osal.h"
#include "sched/interval.h"
#include "dm/dependence_manager.h"
#include "sched/frame_interval.h"
#include "dfx/log/ffrt_log_api.h"

using namespace testing;
using namespace testing::ext;
using namespace ffrt;

class DeadlineTest : public testing::Test {
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
 * @tc.name: qos_interval_create_test
 * @tc.desc: Test whether the qos_interval_create interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_create_test, TestSize.Level1)
{
    uint64_t deadline_us = 50000;
    qos qos = qos_deadline_request;

    interval qi = qos_interval_create(deadline_us, qos);
    EXPECT_NE(&qi, nullptr);

    qos = qos_user_interactive;
    interval qi1 = qos_interval_create(deadline_us, qos);
    EXPECT_NE(&qi1, nullptr);
}

/**
 * @tc.name: qos_interval_destroy_test
 * @tc.desc: Test whether the qos_interval_destroy interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_destroy_test, TestSize.Level1)
{
    interval* qi = new interval();
    qos_interval_destroy(*qi);

    uint64_t deadline_us = 50000;
    qos qos = qos_user_interactive;

    interval qi1 = qos_interval_create(deadline_us, qos);
    qos_interval_destroy(qi1);
}

/**
 * @tc.name: qos_interval_begin_test
 * @tc.desc: Test whether the qos_interval_begin interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_begin_test, TestSize.Level1)
{
    interval* qi = new interval();
    qos_interval_begin(*qi);

    uint64_t deadline_us = 50000;
    qos qos = qos_user_interactive;

    interval qi1 = qos_interval_create(deadline_us, qos);
    qos_interval_begin(qi1);
}

/**
 * @tc.name: qos_interval_update_test
 * @tc.desc: Test whether the qos_interval_update interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_update_test, TestSize.Level1)
{
    uint64_t deadline_us = 50000;
    qos qos = qos_user_interactive;
    uint64_t new_deadline_us = 40000;
    interval* qi = new interval();
    qos_interval_update(*qi, new_deadline_us);

    interval qi1 = qos_interval_create(deadline_us, qos);
    qos_interval_update(qi1, new_deadline_us);
}

/**
 * @tc.name: qos_interval_end_test
 * @tc.desc: Test whether the qos_interval_end interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_end_test, TestSize.Level1)
{
    uint64_t deadline_us = 50000;
    qos qos = qos_user_interactive;
    interval* qi = new interval();
    qos_interval_end(*qi);

    interval qi1 = qos_interval_create(deadline_us, qos);
    qos_interval_end(qi1);
}

/**
 * @tc.name: qos_interval_join_test
 * @tc.desc: Test whether the qos_interval_join interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_join_test, TestSize.Level1)
{
    uint64_t deadline_us = 50000;
    qos qos = qos_deadline_request;
    interval ret = qos_interval_create(deadline_us, qos);
    qos_interval_join(ret);

    qos = qos_user_interactive;
    interval ret1 = qos_interval_create(deadline_us, qos);
    qos_interval_join(ret1);
}

/**
 * @tc.name: qos_interval_leave_test
 * @tc.desc: Test whether the qos_interval_leave interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_leave_test, TestSize.Level1)
{
    uint64_t deadline_us = 50000;
    qos qos = qos_deadline_request;
    interval ret = qos_interval_create(deadline_us, qos);
    qos_interval_leave(ret);

    qos = qos_user_interactive;
    interval ret1 = qos_interval_create(deadline_us, qos);
    qos_interval_leave(ret1);
}
