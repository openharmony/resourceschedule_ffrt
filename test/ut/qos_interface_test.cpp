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
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/types.h>
#include "eu/qos_interface.h"
#include "ffrt_inner.h"
#include "common.h"

#define GET_TID() syscall(SYS_gettid)

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class QosInterfaceTest : public testing::Test {
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
 * @tc.name: FFRTEnableRtg_test
 * @tc.desc: Test whether the FFRTEnableRtg interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTEnableRtgTest, TestSize.Level1)
{
    bool flag = false;
    EXPECT_EQ(0, FFRTEnableRtg(flag));

    flag = true;
    EXPECT_EQ(0, FFRTEnableRtg(flag));
}

/**
 * @tc.name: FFRTAuthEnable_test
 * @tc.desc: Test whether the FFRTAuthEnable interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTAuthEnableTest, TestSize.Level1)
{
    unsigned int uid = getpid();
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;

    EXPECT_EQ(0, FFRTAuthEnable(uid, uaFlag, status));
}

/**
 * @tc.name: FFRTAuthSwitch_test
 * @tc.desc: Test whether the FFRTAuthSwitch interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTAuthSwitchTest, TestSize.Level1)
{
    unsigned int uid = getpid();
    unsigned int rtgFlag = 0x1fff;
    unsigned int qosFlag = 0x0003;
    unsigned int status = 3;

    EXPECT_EQ(0, FFRTAuthSwitch(uid, rtgFlag, qosFlag, status));
}

/**
 * @tc.name: FFRTAuthDelete_test
 * @tc.desc: Test whether the FFRTAuthDelete interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTAuthDeleteTest, TestSize.Level1)
{
    unsigned int uid = getpid();

    EXPECT_EQ(0, FFRTAuthDelete(uid));
}

/**
 * @tc.name: FFRTAuthPause_test
 * @tc.desc: Test whether the FFRTAuthPause interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTAuthPauseTest, TestSize.Level1)
{
    unsigned int uid = getpid();
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;
    FFRTAuthEnable(uid, uaFlag, status);

    EXPECT_EQ(0, FFRTAuthPause(uid));
}

/**
 * @tc.name: FFRTAuthGet_test
 * @tc.desc: Test whether the FFRTAuthGet interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTAuthGetTest, TestSize.Level1)
{
    unsigned int uid = getpid();
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;

    FFRTAuthEnable(uid, uaFlag, status);
    EXPECT_EQ(0, FFRTAuthGet(uid, &uaFlag, &status));
}

/**
 * @tc.name: FFRTQosApply_test
 * @tc.desc: Test whether the FFRTQosApply interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTQosApplyTest, TestSize.Level1)
{
    unsigned int level = 1;

    EXPECT_EQ(0, FFRTQosApply(level));
}

/**
 * @tc.name: FFRTQosApplyForOther_test
 * @tc.desc: Test whether the FFRTQosApplyForOther interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTQosApplyForOtherTest, TestSize.Level1)
{
    unsigned int level = 1;
    int tid = GET_TID();

    EXPECT_EQ(0, FFRTQosApplyForOther(level, tid));
}

/**
 * @tc.name: FFRTQosLeave_test
 * @tc.desc: Test whether the FFRTQosLeave interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTQosLeaveTest, TestSize.Level1)
{
    EXPECT_EQ(0, FFRTQosLeave());
}

/**
 * @tc.name: FFRTQosLeaveForOther_test
 * @tc.desc: Test whether the FFRTQosLeaveForOther interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, FFRTQosLeaveForOtherTest, TestSize.Level1)
{
    unsigned int level = 1;
    int tid = GET_TID();
    FFRTQosApplyForOther(level, tid);

    EXPECT_EQ(0, FFRTQosLeaveForOther(tid));
}

/**
 * @tc.name: QosPolicyTest
 * @tc.desc: Test whether the QosPolicy interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(QosInterfaceTest, QosPolicyTest, TestSize.Level1)
{
    struct QosPolicyData qp = {0, 0, 0, 0, 0};
    struct QosPolicyDatas policyDatas = {0, 0, {qp}};

    EXPECT_EQ(-1, QosPolicy(&policyDatas));
}
