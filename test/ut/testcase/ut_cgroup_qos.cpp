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
#ifndef WITH_NO_MOCKER
#include <mockcpp/mockcpp.hpp>
#endif
#include <fcntl.h>
#include "eu/osattr_manager.h"
#ifndef WITH_NO_MOCKER
#include "eu/qos_config.h"
#endif
#include "eu/qos_interface.h"
#include "../common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class CgroupQosTest : public testing::Test {
protected:
    static void SetUpTestCase()
    {
#ifndef WITH_NO_MOCKER
        MOCKER(write).stubs().will(returnValue(0));
        MOCKER(read).stubs().will(returnValue(0));
#endif
    }

    static void TearDownTestCase()
    {
#ifndef WITH_NO_MOCKER
        GlobalMockObject::verify();
#endif
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

#ifndef FFRT_GITEE
HWTEST_F(CgroupQosTest, UpdateSchedAttr_test, TestSize.Level1)
{
        ffrt_os_sched_attr attr = {100, 10, 99, 99, 9, "2-3"};
        int ret = 0;
#ifndef WITH_NO_MOCKER
        ret = OSAttrManager::Instance()->UpdateSchedAttr(QoS(static_cast<int>(qos_defined_ive)), &attr);
        EXPECT_EQ(ret, 0);
#endif
        ret = OSAttrManager::Instance()->UpdateSchedAttr(QoS(static_cast<int>(qos_user_interactive)), &attr);
        EXPECT_EQ(ret, -1);
}
#endif

HWTEST_F(CgroupQosTest, SetTidToCGroup_test, TestSize.Level1)
{
        int32_t pid = 100;
        OSAttrManager::Instance()->SetTidToCGroup(pid);
        OSAttrManager::Instance()->SetTidToCGroup(-1);
        EXPECT_EQ(pid, 100);
}

HWTEST_F(CgroupQosTest, SetCGroupCtlPara_test, TestSize.Level1)
{
        int32_t value = 1;
        OSAttrManager::Instance()->SetCGroupCtlPara("", value);
        OSAttrManager::Instance()->SetCGroupCtlPara("test", value);
        EXPECT_EQ(value, 1);
}

HWTEST_F(CgroupQosTest, SetCGroupSetPara_test, TestSize.Level1)
{
        std::string value = "1";
        OSAttrManager::Instance()->SetCGroupSetPara("", value);
        OSAttrManager::Instance()->SetCGroupSetPara("test", value);
        EXPECT_EQ(value, "1");
}

HWTEST_F(CgroupQosTest, SetTidToCGroupPrivate_test, TestSize.Level1)
{
        int32_t pid = 100;
        OSAttrManager::Instance()->SetTidToCGroupPrivate("test", pid);
        OSAttrManager::Instance()->SetTidToCGroupPrivate("test", -1);
        EXPECT_EQ(pid, 100);
}

HWTEST_F(CgroupQosTest, SetCGroupPara_test, TestSize.Level1)
{
        int a = 100;
        OSAttrManager::Instance()->SetCGroupPara("/proc/cpuinfo", a);
        EXPECT_EQ(a, 100);
}

HWTEST_F(CgroupQosTest, SetCGroupPara_err_test, TestSize.Level1)
{
#ifndef WITH_NO_MOCKER
        MOCKER(write).stubs().will(returnValue(0));
        MOCKER(read).stubs().will(returnValue(0));
#endif
        int a = 3;
        OSAttrManager::Instance()->SetCGroupPara("/proc/cpuinfo", a);
        EXPECT_EQ(a, 3);
#ifndef WITH_NO_MOCKER
        MOCKER(write).stubs().will(returnValue(-1));
        MOCKER(read).stubs().will(returnValue(0));
#endif
        OSAttrManager::Instance()->SetCGroupPara("/proc/cpuinfo", a);
#ifndef WITH_NO_MOCKER
        MOCKER(write).stubs().will(returnValue(0));
        MOCKER(read).stubs().will(returnValue(33));
#endif
        OSAttrManager::Instance()->SetCGroupPara("/proc/cpuinfo", a);
}

class QosTest : public testing::Test {
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

HWTEST_F(QosTest, QosConfig_test, TestSize.Level1)
{
#ifndef WITH_NO_MOCKER
    int i = 0;
    auto handle = ffrt::submit_h([]{
        QosConfig::Instance().setPolicySystem();
        i++;
    });
    EXPECT_EQ(i, 1);
#endif
}

class QosInterfaceTest_2 : public testing::Test {
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

HWTEST_F(QosInterfaceTest_2, QosPolicyTest, TestSize.Level1)
{
    struct QosPolicyData qp = {0, 0, 0, 0, 0};
    struct QosPolicyDatas policyDatas = {0, 0, {qp}};

    int ret = QosPolicy(&policyDatas);
    EXPECT_NE(ret, 0);
}

HWTEST_F(QosInterfaceTest_2, FFRTEnableRtgTest, TestSize.Level1)
{
    bool flag = false;
    FFRTEnableRtg(flag);
    EXPECT_EQ(flag, false);
}

HWTEST_F(QosInterfaceTest_2, FFRTAuthEnableTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;
    FFRTAuthEnable(uid, uaFlag, status);
    EXPECT_EQ(status, 3);
}

HWTEST_F(QosInterfaceTest_2, FFRTAuthSwitchTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int rtgFlag = 0x1fff;
    unsigned int qosFlag = 0x0003;
    unsigned int status = 3;
    FFRTAuthSwitch(uid, rtgFlag, qosFlag, status);
    EXPECT_EQ(status, 3);
}

HWTEST_F(QosInterfaceTest_2, FFRTAuthDeleteTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    FFRTAuthDelete(uid);
    EXPECT_EQ(uid, 3039);
}

HWTEST_F(QosInterfaceTest_2, FFRTAuthPauseTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;
    FFRTAuthEnable(uid, uaFlag, status);
    FFRTAuthPause(uid);
    EXPECT_EQ(uid, 3039);
}

HWTEST_F(QosInterfaceTest_2, FFRTAuthGetTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;
    int ret = 0;
    ret = FFRTAuthEnable(uid, uaFlag, status);
    FFRTAuthGet(uid, &uaFlag, &status);

    if (ret < 0) {
        EXPECT_EQ(status, 0);
    } else {
        EXPECT_EQ(status, 3);
    }
}

HWTEST_F(QosInterfaceTest_2, FFRTQosApplyTest, TestSize.Level1)
{
    unsigned int level = 1;

    FFRTQosApply(level);
    EXPECT_EQ(level, 1);
}

HWTEST_F(QosInterfaceTest_2, FFRTQosApplyForOtherTest, TestSize.Level1)
{
    unsigned int level = 1;
    int tid = 0;

    FFRTQosApplyForOther(level, tid);
    EXPECT_EQ(level, 1);
}

HWTEST_F(QosInterfaceTest_2, FFRTQosLeaveTest, TestSize.Level1)
{
    int ret = FFRTQosLeave();
    EXPECT_EQ(ret, 0);
}

HWTEST_F(QosInterfaceTest_2, FFRTQosLeaveForOtherTest, TestSize.Level1)
{
    unsigned int level = 1;
    int tid = 0;
    FFRTQosApplyForOther(level, tid);

    FFRTQosLeaveForOther(tid);
    EXPECT_EQ(level, 1);
}

HWTEST_F(QosInterfaceTest_2, FFRTQosConvertInt, TestSize.Level1)
{
    QoS qos1 = 1;
    QoS qos2 = 2;
    QoS qos3 = qos1 + qos2;
    printf("qos3=%d", qos3());
    EXPECT_EQ(qos3, 3);
}
