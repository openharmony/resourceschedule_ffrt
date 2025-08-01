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
#include <cstring>
#include <algorithm>
#include <sched.h>
#include <unistd.h>
#include "ffrt_inner.h"
#include "c/queue_ext.h"
#include "../common.h"
#include "util/white_list.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class WhiteListTest : public testing::Test {
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

/*
 * 测试用例名称 : whiteList_enable
 * 测试用例描述：进程在白名单内
 * 操作步骤    ：1、调用接口查看是否在白名单内
 * 预期结果    ：接口返回True
 */
HWTEST_F(WhiteListTest, whiteList_enable, TestSize.Level1)
{
    int x = 0;
    if (WhiteList::GetInstance().IsEnabled("WhiteListTest", true)) {
        x += 1;
    }
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : whiteList_default
 * 测试用例描述：在白名单内且在异常场景下，默认返回True
 * 操作步骤    ：1、调用接口查看是否在白名单内，或是否处于异常场景
 * 预期结果    ：接口返回True
 */
HWTEST_F(WhiteListTest, whiteList_default, TestSize.Level1)
{
    int x = 0;
    if (WhiteList::GetInstance().IsEnabled("WhiteListTest", true)) {
        x += 1;
    }

    if (WhiteList::GetInstance().IsEnabled("WhiteListTest123", true)) {
        x += 1;
    }
    EXPECT_EQ(x, 2);
}

/*
 * 测试用例名称 : whiteList_default_reverse
 * 测试用例描述：异常情况下，在白名单内且默认皆返回false
 * 操作步骤    ：1、调用接口查看是否在白名单内，或是否处于异常场景
 * 预期结果    ：接口返回false
 */
HWTEST_F(WhiteListTest, whiteList_default_reverse, TestSize.Level1)
{
    int x = 0;
    if (WhiteList::GetInstance().IsEnabled("WhiteListTest1", false)) {
        x += 1;
    }
    EXPECT_EQ(x, 0);
}