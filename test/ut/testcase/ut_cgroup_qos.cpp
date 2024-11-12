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

    virtual void SetUp()
    {
    }

    virtual void TearDown()
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
        OSAttrManager::Instance()->SetTidToCGroup(100);
        OSAttrManager::Instance()->SetTidToCGroup(-1);
}

HWTEST_F(CgroupQosTest, SetCGroupCtlPara_test, TestSize.Level1)
{
        OSAttrManager::Instance()->SetCGroupCtlPara("", 1);
        OSAttrManager::Instance()->SetCGroupCtlPara("test", 1);
}

HWTEST_F(CgroupQosTest, SetCGroupSetPara_test, TestSize.Level1)
{
        OSAttrManager::Instance()->SetCGroupSetPara("", "1");
        OSAttrManager::Instance()->SetCGroupSetPara("test", "1");
}

HWTEST_F(CgroupQosTest, SetTidToCGroupPrivate_test, TestSize.Level1)
{
        OSAttrManager::Instance()->SetTidToCGroupPrivate("test", 100);
        OSAttrManager::Instance()->SetTidToCGroupPrivate("test", -1);
}

HWTEST_F(CgroupQosTest, SetCGroupPara_test, TestSize.Level1)
{
        int a = 100;
        OSAttrManager::Instance()->SetCGroupPara("/proc/cpuinfo", a);
}

HWTEST_F(CgroupQosTest, SetCGroupPara_err_test, TestSize.Level1)
{
#ifndef WITH_NO_MOCKER
        MOCKER(write).stubs().will(returnValue(0));
        MOCKER(read).stubs().will(returnValue(0));
#endif
        int a = 3;
        OSAttrManager::Instance()->SetCGroupPara("/proc/cpuinfo", a);
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

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

HWTEST_F(QosTest, QosConfig_test, TestSize.Level1)
{
#ifndef WITH_NO_MOCKER
        QosConfig::Instance().setPolicySystem();
#endif
}

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

HWTEST_F(QosInterfaceTest, QosPolicyTest, TestSize.Level1)
{
    struct QosPolicyData qp = {0, 0, 0, 0, 0};
    struct QosPolicyDatas policyDatas = {0, 0, {qp}};

    QosPolicy(&policyDatas);
}

HWTEST_F(QosInterfaceTest, FFRTEnableRtgTest, TestSize.Level1)
{
    bool flag = false;
    FFRTEnableRtg(flag);
}

HWTEST_F(QosInterfaceTest, FFRTAuthEnableTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;
    FFRTAuthEnable(uid, uaFlag, status);
}

HWTEST_F(QosInterfaceTest, FFRTAuthSwitchTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int rtgFlag = 0x1fff;
    unsigned int qosFlag = 0x0003;
    unsigned int status = 3;
    FFRTAuthSwitch(uid, rtgFlag, qosFlag, status);
}

HWTEST_F(QosInterfaceTest, FFRTAuthDeleteTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    FFRTAuthDelete(uid);
}

HWTEST_F(QosInterfaceTest, FFRTAuthPauseTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;
    FFRTAuthEnable(uid, uaFlag, status);
    FFRTAuthPause(uid);
}

HWTEST_F(QosInterfaceTest, FFRTAuthGetTest, TestSize.Level1)
{
    unsigned int uid = 3039;
    unsigned int uaFlag = 0x1fff;
    unsigned int status = 3;

    FFRTAuthEnable(uid, uaFlag, status);
    FFRTAuthGet(uid, &uaFlag, &status);
}

HWTEST_F(QosInterfaceTest, FFRTQosApplyTest, TestSize.Level1)
{
    unsigned int level = 1;

    FFRTQosApply(level);
}

HWTEST_F(QosInterfaceTest, FFRTQosApplyForOtherTest, TestSize.Level1)
{
    unsigned int level = 1;
    int tid = 0;

    FFRTQosApplyForOther(level, tid);
}

HWTEST_F(QosInterfaceTest, FFRTQosLeaveTest, TestSize.Level1)
{
    FFRTQosLeave();
}

HWTEST_F(QosInterfaceTest, FFRTQosLeaveForOtherTest, TestSize.Level1)
{
    unsigned int level = 1;
    int tid = 0;
    FFRTQosApplyForOther(level, tid);

    FFRTQosLeaveForOther(tid);
}

HWTEST_F(QosInterfaceTest, FFRTQosConvertInt, TestSize.Level1)
{
    QoS qos1 = 1;
    QoS qos2 = 2;
    QoS qos3 = qos1 + qos2;
    printf("qos3=%d", qos3());
}