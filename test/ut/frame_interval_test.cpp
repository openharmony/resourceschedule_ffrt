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
#include "dfx/log/ffrt_log_api.h"
#include "sched/workgroup_internal.h"
#include "eu/execute_unit.h"

#define private public
#include "sched/frame_interval.h"
#undef private
#include "common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class FrameIntervalTest : public testing::Test {
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
 * @tc.name: FrameIntervalTest
 * @tc.desc: Test whether the FrameInterval interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(FrameIntervalTest, FrameIntervalTest, TestSize.Level0)
{
    FrameInterval* fi = new FrameInterval(100000, QoS(5));
    EXPECT_NE(fi, nullptr);
    delete fi;
}

/**
 * @tc.name: OnQoSIntervalsTest
 * @tc.desc: Test whether the OnQoSIntervals interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(FrameIntervalTest, OnQoSIntervalsTest, TestSize.Level0)
{
    FrameInterval* fi = new FrameInterval(100000, QoS(5));
    fi->OnQoSIntervals(ffrt::IntervalState::DEADLINE_BEGIN);
    fi->OnQoSIntervals(ffrt::IntervalState::DEADLINE_END);
    EXPECT_NE(fi, nullptr);
    delete fi;
}

/**
 * @tc.name: BeginTest
 * @tc.desc: Test whether the Begin interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(FrameIntervalTest, BeginTest, TestSize.Level0)
{
    FrameInterval* fi = new FrameInterval(100000, QoS(5));
    int ret = fi->Begin();
    EXPECT_EQ(0, ret);

    int ret1 = fi->Begin();
    EXPECT_EQ(-1, ret1);
    delete fi;
}

/**
 * @tc.name: EndTest
 * @tc.desc: Test whether the End interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(FrameIntervalTest, EndTest, TestSize.Level0)
{
    FrameInterval* fi = new FrameInterval(100000, QoS(5));
    fi->End();
    EXPECT_FALSE(fi->isBegin);

    fi->isBegin = true;
    fi->End();
    EXPECT_FALSE(fi->isBegin);
    delete fi;
}
/**
 * @tc.name: updateTest
 * @tc.desc: Test whether the update interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(FrameIntervalTest, updateTest, TestSize.Level0)
{
    FrameInterval* fi = new FrameInterval(100000, QoS(5));
    EXPECT_NE(fi, nullptr);
    uint64_t deadline = 900;
    fi->Update(deadline);
    deadline = 1500000;
    fi->Update(deadline);

    deadline = 100000;
    fi->Update(deadline);
    delete fi;
}

/**
 * @tc.name: JoinTest
 * @tc.desc: Test whether the Join interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(FrameIntervalTest, JoinTest, TestSize.Level0)
{
    FrameInterval* fi = new FrameInterval(100000, QoS(5));
    EXPECT_NE(fi, nullptr);
    fi->Join();
    fi->Leave();
    delete fi;
}
