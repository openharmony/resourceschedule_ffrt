/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "cpp/qos_convert.h"
#if defined(QOS_FRAME_RTG)
#include "concurrent_task_client.h"
#endif
#include "eu/qos_interface.h"
#include "common.h"

namespace OHOS {
namespace FFRT_TEST {
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace OHOS::FFRT_TEST;
using namespace ffrt;
using namespace std;
#if defined(QOS_FRAME_RTG)
using namespace OHOS::ConcurrentTask;
#endif

#define QOS_CTRL_SET_QOS_THREAD _IOWR('q', 2, struct TaskConfig)

struct TaskConfig {
    int32_t pid;
    int32_t value;
};

const string QOS_CTRL_FILE_PATH = "/dev/qos_sched_ctrl";
const int NR_QOS_LEVEL = 9;
constexpr int ERROR_NUM = -1;

class QosConvertTest : public testing::Test {
public:
    bool IsLinuxOs()
    {
        struct utsname nameData;
        uname(&nameData);
        int cmpNum = 5;
        return strncmp(nameData.sysname, "Linux", cmpNum) == 0 ? true : false;
    }

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

static int SetRssQos(int level)
{
    int tid = gettid();
    if (level < -1 || level > 9) {
        return ERROR_NUM;
    }
    int32_t handle = open(QOS_CTRL_FILE_PATH.c_str(), (O_RDWR | O_CLOEXEC));
    if (handle <= 0) {
        printf("invalid handle %d\n", static_cast<int>(handle));
        return ERROR_NUM;
    }

    struct TaskConfig threadData = {tid, level};
    int ret = ioctl(handle, QOS_CTRL_SET_QOS_THREAD, &threadData);
    if (ret != 0) {
        printf("ioctl setQos failed\n");
        return ERROR_NUM;
    }

    return ret;
}

static int QosTransfer(int qos)
{
    switch (qos) {
        case 9:
        case 8:
        case 7: return 5;
        case 6: return 4;
        case 5:
        case 4: return 3;
        case 3: return 2;
        case 2: return 1;
        case 1: return 0;
        default:
            return -1;
    }
}

#ifndef FFRT_GITEE
HWTEST_F(QosConvertTest, GetStaticQos1, TestSize.Level0)
{
    for (int i = 1; i <= NR_QOS_LEVEL; i++) {
        qos tmpQos = qos_default;
        SetRssQos(i);
        int ret = GetStaticQos(tmpQos);
        if (!IsLinuxOs()) {
            return;
        }
        EXPECT_EQ(ret, 0);
        EXPECT_EQ(tmpQos, QosTransfer(i));
    }
}
#endif

HWTEST_F(QosConvertTest, GetStaticQos2, TestSize.Level1)
{
    qos tmpQos = qos_default;
    SetRssQos(-1);
    int ret = GetStaticQos(tmpQos);
    EXPECT_EQ(tmpQos, qos_default);
    EXPECT_EQ(ret, -1);

    SetRssQos(0);
    ret = GetStaticQos(tmpQos);
    EXPECT_EQ(tmpQos, qos_default);
    EXPECT_EQ(ret, -1);

    SetRssQos(10);
    ret = GetStaticQos(tmpQos);
    EXPECT_EQ(tmpQos, qos_default);
    EXPECT_EQ(ret, -1);

    SetRssQos(-2);
    ret = GetStaticQos(tmpQos);
    EXPECT_EQ(tmpQos, qos_default);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(QosConvertTest, GetDynamicQos, TestSize.Level0)
{
#if defined(QOS_FRAME_RTG)
    SetRssQos(8);
    qos tmpQos = qos_default;
    int ret = GetDynamicQos(tmpQos);
    IntervalReply rs;
    ConcurrentTaskClient::GetInstance().QueryInterval(QUERY_RENDER_SERVICE, rs);
    EXPECT_EQ(tmpQos, qos_default);
#endif
}

#ifndef FFRT_GITEE
HWTEST_F(QosConvertTest, FFRTQosGetInterface, TestSize.Level0)
{
    SetRssQos(8);
    struct QosCtrlData data;
    int ret = FFRTQosGet(data);
    if (!IsLinuxOs()) {
        return;
    }
    EXPECT_EQ(data.staticQos, 5);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(QosConvertTest, FFRTQosGetForOtherInterface, TestSize.Level0)
{
    SetRssQos(8);
    struct QosCtrlData data;
    int ret = FFRTQosGetForOther(gettid(), data);
    if (!IsLinuxOs()) {
        return;
    }
    EXPECT_EQ(data.staticQos, 5);
    EXPECT_EQ(ret, 0);
}
#endif
}
}