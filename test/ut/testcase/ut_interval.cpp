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
#include <climits>


#define private public
#define protect public
#include "sched/interval.h"
#include "sched/frame_interval.h"
#include "sched/load_tracking.h"
#undef private
#undef protect
#include "../common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class IntervalTest : public testing::Test {
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

HWTEST_F(IntervalTest, deadline_test, TestSize.Level0)
{
    Deadline dl(0);
    EXPECT_EQ(dl.ToNs(), 1);
    EXPECT_EQ(dl.ToUs(), 1);
    EXPECT_EQ(dl.ToMs(), 1);

    dl.Update(1000);
    EXPECT_EQ(dl.ToNs(), 1000000);
    EXPECT_EQ(dl.ToUs(), 1000);
    EXPECT_EQ(dl.ToMs(), 1);

    dl.Update(1000000);
    EXPECT_NE(dl.LeftNs(), 1);

    std::this_thread::sleep_for(std::chrono::milliseconds(1001));

    EXPECT_EQ(dl.LeftNs(), 1);
}

HWTEST_F(IntervalTest, simple_load_predictor_test, TestSize.Level0)
{
    std::initializer_list<std::pair<int, int>> table = {
        {10, 10},
        {100, 100},
        {300, 300},
        {50, 300},
        {3000, 3000},
        {900, 3000},
        {30, 900},
        {200, 836},
        {0, 826},
        {5000, 5000},
        {10240, 10240},
        {25600, 25600},
        {40, 25600},
        {300, 8236},
        {50, 7246},
    };

    SimpleLoadPredictor lp;
    for (auto& it : table) {
        lp.UpdateLoad(it.first);
        EXPECT_EQ(lp.GetPredictLoad(), it.second);
    }

    lp.Clear();
    EXPECT_EQ(lp.GetPredictLoad(), 0);
}

HWTEST_F(IntervalTest, interval_basic_test, TestSize.Level0)
{
    DefaultInterval interval = DefaultInterval(100, QoS(static_cast<int>(qos_deadline_request)));
    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    interval.End();

    ret = interval.Begin();
    interval.Update(50);

    EXPECT_EQ(ret, 0);
    usleep(10);
    interval.CheckPoint();
    interval.End();
}

HWTEST_F(IntervalTest, interval_join_test, TestSize.Level0)
{
    DefaultInterval interval = DefaultInterval(100, QoS(static_cast<int>(qos_deadline_request)));
    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);

    interval.Join();
    interval.Leave();

    interval.End();
}

HWTEST_F(IntervalTest, interval_begin_exception_test, TestSize.Level0)
{
    // case interval begin while last interval not end
    DefaultInterval interval = DefaultInterval(100, QoS(static_cast<int>(qos_deadline_request)));
    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    ret = interval.Begin();
    EXPECT_EQ(ret, -1);
    interval.End();
    ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    interval.End();
}

HWTEST_F(IntervalTest, interval_end_exception_test, TestSize.Level0)
{
    // case interval function called before begin
    DefaultInterval interval = DefaultInterval(100, QoS(static_cast<int>(qos_deadline_request)));

    interval.Update(50);
    interval.CheckPoint();
    interval.End();

    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    interval.End();
}

HWTEST_F(IntervalTest, fInterval_basic_test, TestSize.Level0)
{
    FrameInterval interval = FrameInterval(100, QoS(static_cast<int>(qos_user_interactive)));
    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    interval.End();

    ret = interval.Begin();
    interval.Update(50);

    EXPECT_EQ(ret, 0);
    usleep(10);
    interval.CheckPoint();
    interval.End();
}

HWTEST_F(IntervalTest, fInterval_join_test, TestSize.Level0)
{
    FrameInterval interval = FrameInterval(100, QoS(static_cast<int>(qos_user_interactive)));
    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);

    interval.Join();
    interval.Leave();

    interval.End();
}

HWTEST_F(IntervalTest, fInterval_repeat_begin_exception_test, TestSize.Level0)
{
    // case interval begin while last interval not end
    FrameInterval interval = FrameInterval(100, QoS(static_cast<int>(qos_user_interactive)));
    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    ret = interval.Begin();
    EXPECT_EQ(ret, -1);
    interval.End();
    ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    interval.End();
}

HWTEST_F(IntervalTest, fInterval_function_before_begin_exception_test, TestSize.Level0)
{
    // case interval function called before begin
    FrameInterval interval = FrameInterval(100, QoS(static_cast<int>(qos_user_interactive)));

    interval.Update(50);
    interval.CheckPoint();
    interval.End();

    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);
    interval.End();
}

HWTEST_F(IntervalTest, loadPredict_basic_test, TestSize.Level0)
{
    IntervalLoadPredictor lp;

    lp.UpdateTotalLoad(100);
    lp.UpdateTotalLoad(200);
    lp.UpdateTotalLoad(300);

    uint64_t load = lp.GetTotalLoad();
    // max among average and recent 2 load
    EXPECT_EQ(load, 300);
}

HWTEST_F(IntervalTest, update_task_test, TestSize.Level0)
{
    FrameInterval interval = FrameInterval(100, QoS(static_cast<int>(qos_user_interactive)));
    int ret = interval.Begin();
    EXPECT_EQ(ret, 0);

    TaskSwitchState state = TaskSwitchState::BEGIN;

    interval.UpdateTaskSwitch(state);
    state = TaskSwitchState::UPDATE;
    interval.UpdateTaskSwitch(state);
    state = TaskSwitchState::END;
    interval.UpdateTaskSwitch(state);
}