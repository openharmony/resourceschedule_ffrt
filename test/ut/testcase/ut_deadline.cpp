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
#include "sched/interval.h"
#include "sched/sched_deadline.h"
#include "tm/cpu_task.h"
#include "../common.h"

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


HWTEST_F(DeadlineTest, interval_deadline_test, TestSize.Level1)
{
    auto it = qos_interval_create(100, static_cast<int>(ffrt::qos_user_interactive));
    int ret = qos_interval_begin(it);
    EXPECT_EQ(ret, 0);

    qos_interval_end(it);

    ret = qos_interval_begin(it);
    EXPECT_EQ(ret, 0);

    qos_interval_update(it, 50);
    qos_interval_end(it);
    qos_interval_destroy(it);
}

HWTEST_F(DeadlineTest, interval_join_test, TestSize.Level1)
{
    auto it = qos_interval_create(100, static_cast<int>(ffrt::qos_user_interactive));
    int ret = qos_interval_begin(it);
    EXPECT_EQ(ret, 0);

    qos_interval_join(it);
    qos_interval_leave(it);

    qos_interval_end(it);
    qos_interval_destroy(it);
}

HWTEST_F(DeadlineTest, interval_exception_test, TestSize.Level1)
{
    auto it = qos_interval_create(100, static_cast<int>(ffrt::qos_user_interactive));
    int ret = qos_interval_begin(it);
    EXPECT_EQ(ret, 0);

    ret = qos_interval_begin(it);
    EXPECT_EQ(ret, -1);

    qos_interval_end(it);

    ret = qos_interval_begin(it);
    EXPECT_EQ(ret, 0);
    qos_interval_destroy(it);
}

HWTEST_F(DeadlineTest, interval_exception2_test, TestSize.Level1)
{
    auto it = qos_interval_create(100, static_cast<int>(ffrt::qos_user_interactive));

    qos_interval_update(it, 50);
    qos_interval_end(it);

    int ret = qos_interval_begin(it);
    EXPECT_EQ(ret, 0);

    qos_interval_end(it);
    qos_interval_destroy(it);
}

HWTEST_F(DeadlineTest, interval_exception3_test, TestSize.Level1)
{
    auto it = qos_interval_create(100, static_cast<int>(ffrt::qos_default));

    qos_interval_update(it, 50);
    qos_interval_end(it);

    int ret = qos_interval_begin(it);
    EXPECT_EQ(ret, -1);

    qos_interval_join(it);
    qos_interval_leave(it);

    qos_interval_end(it);
    qos_interval_destroy(it);
}

HWTEST_F(DeadlineTest, sched_deadline_test, TestSize.Level1)
{
    CPUEUTask *ctx = ExecuteCtx::Cur()->task;
    TaskLoadTracking::Begin(ctx);
    TaskLoadTracking::End(ctx);

    uint64_t load = TaskLoadTracking::GetLoad(ctx);
    EXPECT_EQ(load, 0);
}

