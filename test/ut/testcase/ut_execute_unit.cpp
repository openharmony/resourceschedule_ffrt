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

#include <random>

#include <gtest/gtest.h>

#include "internal_inc/config.h"
#include "eu/cpu_worker.h"
#include "eu/cpu_monitor.h"
#include "core/entity.h"
#include "sched/scheduler.h"
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

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

// 提交、取消并行普通/延时任务成功
HWTEST_F(ExecuteUnitTest, submit_cancel_succ, TestSize.Level1)
{
    std::atomic<int> x = 0;
    ffrt::submit([&]() { x.fetch_add(1); });
    ffrt::submit([&]() { x.fetch_add(2); }, {}, {}, ffrt::task_attr().delay(1));
    auto h1 = ffrt::submit_h([&]() { x.fetch_add(3); });
    auto h2 = ffrt::submit_h([&]() { x.fetch_add(4); }, {}, {}, ffrt::task_attr().delay(2));
    int cancel_ret = ffrt::skip(h2);
    EXPECT_EQ(cancel_ret, 0);
    ffrt::wait();
    EXPECT_EQ(x.load(), 6);
}

// 提交、取消并行普通/延时任务成功
HWTEST_F(ExecuteUnitTest, submit_cancel_failed, TestSize.Level1)
{
    int x = 0;
    auto h1 = ffrt::submit_h([&]() { x += 1; });
    auto h2 = ffrt::submit_h([&]() { x += 2; }, {&x}, {&x}, ffrt::task_attr().delay(1));
    int cancel_ret = ffrt::skip(h2);
    EXPECT_EQ(cancel_ret, 0);
    ffrt::wait();
    EXPECT_EQ(x, 1);

    cancel_ret = ffrt::skip(h1);
    EXPECT_EQ(cancel_ret, 1);
    ffrt::task_handle h3;
    cancel_ret = ffrt::skip(h3);
    EXPECT_EQ(cancel_ret, -1);
}
