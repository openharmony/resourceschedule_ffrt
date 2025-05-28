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

#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

#include <gtest/gtest.h>

#include <sched/workgroup_internal.h>
#include "eu/rtg_ioctl.h"
#include "dfx/log/ffrt_log_api.h"

#ifdef QOS_WORKER_FRAME_RTG
#include "sched/task_client_adapter.h"
#endif

#ifndef WITH_NO_MOCKER
#include <mockcpp/mockcpp.hpp>
#endif

using namespace ffrt;

class MultiWorkgroupTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
    }

    static void TearDownTestCase()
    {
    }

    virtual void SetUp()
    {
    // x86环境下setuid是不生效的
#ifndef WITH_NO_MOCKER
        MOCKER(getuid)
            .stubs()
            .will(returnValue(RS_UID));
#else
        (void)setuid(RS_UID);
#endif
        FFRT_LOGI("test,current uid:%d\n", getuid());
    }

    virtual void TearDown()
    {
    }
};

#ifdef QOS_WORKER_FRAME_RTG
/**
 * @tc.name: Compare_RS_UID
 * @tc.desc: 看护ffrt中定义的RS_UID和QOS层的RS_UID必须相同
 * @tc.type: FUNC
 */
TEST_F(MultiWorkgroupTest, Compare_RS_UID)
{
    FFRT_LOGI("testcase,current uid:%d\n", getuid());
    FFRT_LOGI("Compare_RS_UID enter, 1\n");
    IntervalReply rs;
    rs.rtgId = -1;
    rs.tid = -1;
    CTC_QUERY_INTERVAL(QUERY_RENDER_SERVICE, rs);
    FFRT_LOGI("Compare_RS_UID exit, rtgId:%d\n", rs.rtgId);
    EXPECT_GT(rs.rtgId, 0u);
}

/**
 * @tc.name: RS_JoinWGTest
 * @tc.desc: 看护 JoinWG 中加入RS的WG分组
 * @tc.type: FUNC
 */
TEST_F(MultiWorkgroupTest, RS_JoinWGTest)
{
    int qos = 6;
    int tid = gettid();
    {
        FFRT_LOGI("wg not created, join wg with false return; uid:%d,tid:%d", getuid(), gettid());
        bool ret = JoinWG(tid, qos);
        EXPECT_FALSE(ret);
    }
    {
        WorkGroup* workGroup = WorkgroupCreate(8333, qos);
        FFRT_LOGI("wg created, first join wg with true return;");
        bool ret = JoinWG(tid, qos);
        FFRT_LOGI("wg tids:0:%d,1:%d\n", workGroup->tids[0], workGroup->tids[1]);
        EXPECT_TRUE(ret);
        EXPECT_TRUE(workGroup->tids[0] == tid);
        EXPECT_TRUE(workGroup->tids[1] == -1);
        FFRT_LOGI("join again, with true return,valid tids remain 1\n");
        ret = JoinWG(tid, qos);
        EXPECT_TRUE(ret);
        EXPECT_TRUE(workGroup->tids[0] == tid);
        EXPECT_TRUE(workGroup->tids[1] == -1);
        LeaveWG(tid, qos);
        WorkgroupClear(workGroup);
    }
}

TEST_F(MultiWorkgroupTest, RS_LeaveWGTest)
{
    int qos = 6;
    int tid = gettid();
    {
        // leave before join
        FFRT_LOGI("wg not created,LeaveWG with false return\n");
        bool ret = LeaveWG(tid, qos);
        EXPECT_FALSE(ret);
    }
    WorkGroup* workGroup = WorkgroupCreate(8333, qos);
    {
        // leave after join
        FFRT_LOGI("wg created,LeaveWG directly with true return and tids clean\n");
        bool ret = LeaveWG(tid, qos);
        EXPECT_TRUE(workGroup->tids[0] == -1);
        EXPECT_TRUE(ret);
    }
    {
        FFRT_LOGI("wg created, join first, and then leave with true return\n");
        bool ret = JoinWG(tid, qos);
        EXPECT_TRUE(ret);
        EXPECT_TRUE(workGroup->tids[0] == tid);
        EXPECT_TRUE(workGroup->tids[1] == -1);
        ret = LeaveWG(tid, qos);
        EXPECT_TRUE(workGroup->tids[0] == -1);
        EXPECT_TRUE(ret);
    }
}

TEST_F(MultiWorkgroupTest, RS_WorkgroupCreateTest)
{
    int qos = 6;
    int tid = gettid();
    FFRT_LOGI("first create with not nullptr return\n");
    WorkGroup* workGroup = WorkgroupCreate(8333, qos);
    EXPECT_TRUE(workGroup != nullptr);
    EXPECT_TRUE(workGroup->rtgId > 0);
    FFRT_LOGI("again create with same ptr return\n");
    WorkGroup* workGroup2 = WorkgroupCreate(8333, qos);
    EXPECT_TRUE(workGroup == workGroup2);
    WorkgroupClear(workGroup);
}

TEST_F(MultiWorkgroupTest, RS_WorkgroupClearTest)
{
    int qos = 6;
    int tid = gettid();
    {
        FFRT_LOGI("[WorkgroupClear] wg created, return true\n");
        WorkGroup* workGroup = WorkgroupCreate(8333, qos);
        bool ret = WorkgroupClear(workGroup);
        EXPECT_TRUE(ret);
    }
}

#endif