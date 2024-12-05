/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include <fcntl.h>
#include "eu/qos_interface.h"
#include "cpp/qos_convert.h"
#include "eu/osattr_manager.h"
#include "sync/delayed_worker.h"
#include "util/ffrt_facade.h"
#include "../common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;


class QosConvertTest : public testing::Test {
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

HWTEST_F(QosConvertTest, GetDynamicQosTest, TestSize.Level1)
{
    qos tmpQos = qos_default;
    int ret = GetDynamicQos(tmpQos);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(QosConvertTest, GetStaticQosTest, TestSize.Level1)
{
    qos tmpQos = qos_default;
    int ret = GetStaticQos(tmpQos);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(QosConvertTest, IsDelayerWorkerThreadTest, TestSize.Level1)
{
    const uint64_t timeoutUs = 100;
    DelayedWorker::ThreadEnvCreate();
    sleep(1);
    bool ret = DelayedWorker::IsDelayerWorkerThread();
    EXPECT_EQ(ret, false);
}

class CgroupQosTest : public testing::Test {
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

int TmpFun()
{
    int ret = 6;
    return ret;
}
