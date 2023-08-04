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

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include "cpp/qos_convert.h"
#include "eu/qos_interface.h"

using namespace testing;
using namespace testing::ext;
using namespace ffrt;

class QosConvertTest : public testing::Test {
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

HWTEST_F(QosConvertTest, GetStaticQos, TestSize.Level1)
{
    qos myqos = qos_default;
    int ret = GetStaticQos(myqos);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(myqos, qos_default);
}

HWTEST_F(QosConvertTest, GetDynamicQos, TestSize.Level1)
{
    qos myqos = qos_default;
    int ret = GetDynamicQos(myqos);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(myqos, qos_default);
}