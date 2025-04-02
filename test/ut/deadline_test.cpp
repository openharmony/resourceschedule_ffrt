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
#include "c/deadline.h"
#include "internal_inc/osal.h"
#include "sched/interval.h"
#include "dm/dependence_manager.h"
#include "sched/frame_interval.h"
#include "dfx/log/ffrt_log_api.h"
#include "common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class DeadlineTest : public testing::Test {
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
 * @tc.name: qos_interval_create_test
 * @tc.desc: Test whether the qos_interval_create interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_create_test, TestSize.Level1)
{
    uint64_t deadline_us = 50000;
    qos qos = qos_deadline_request;

    interval qi = qos_interval_create(deadline_us, qos);
    EXPECT_NE(qi, nullptr);
    qos_interval_destroy(qi);

    qos = qos_max + 1;
    interval qi1 = qos_interval_create(deadline_us, qos);
    EXPECT_EQ(qi1, nullptr);
}

/**
 * @tc.name: qos_interval_destroy_test
 * @tc.desc: Test whether the qos_interval_destroy interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(DeadlineTest, qos_interval_destroy_test, TestSize.Level1)
{
    interval* qi = new interval();
    EXPECT_NE(qi, nullptr);
    qos_interval_destroy(*qi);
    delete qi;

    uint64_t deadline_us = 50000;
    qos qos = qos_max + 1;

    interval qi1 = qos_interval_create(deadline_us, qos);
    EXPECT_EQ(qi1, nullptr);
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
    delete qi;

    uint64_t deadline_us = 50000;
    qos qos = qos_max + 1;

    interval qi1 = qos_interval_create(deadline_us, qos);
    EXPECT_EQ(qi1, nullptr);
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
    qos qos = qos_max + 1;
    uint64_t new_deadline_us = 40000;
    interval* qi = new interval();
    qos_interval_update(*qi, new_deadline_us);
    delete qi;

    interval qi1 = qos_interval_create(deadline_us, qos);
    EXPECT_EQ(qi1, nullptr);
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
    qos qos = qos_max + 1;
    interval* qi = new interval();
    qos_interval_end(*qi);
    delete qi;

    interval qi1 = qos_interval_create(deadline_us, qos);
    EXPECT_EQ(qi1, nullptr);
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
    EXPECT_NE(ret, nullptr);
    qos_interval_join(ret);
    ffrt_interval_destroy(ret);

    qos = qos_max + 1;
    interval ret1 = qos_interval_create(deadline_us, qos);
    EXPECT_EQ(ret1, nullptr);
    qos_interval_join(ret1);
    ffrt_interval_destroy(ret1);
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
    EXPECT_NE(ret, nullptr);
    qos_interval_leave(ret);
    qos_interval_destroy(ret);

    qos = qos_max + 1;
    interval ret1 = qos_interval_create(deadline_us, qos);
    EXPECT_EQ(ret1, nullptr);
    qos_interval_leave(ret1);
}
