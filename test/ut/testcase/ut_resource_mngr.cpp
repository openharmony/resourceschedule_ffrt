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
#include "ffrt_inner.h"
#include "../common.h"

extern "C" int ffrt_set_task_io_qos(int service, uint64_t id, int qos, void* payload);

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class ResourceMngrTest : public testing::Test {
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
 * 测试用例名称 : ffrt_set_task_io_qos_succ
 * 测试用例描述：设置任务的IO QoS成功
 * 操作步骤    ：1、设置qos为0
 * 预期结果    ：执行成功
 */
HWTEST_F(ResourceMngrTest, ffrt_set_task_io_qos_succ, TestSize.Level1)
{
    int ret = ffrt_set_task_io_qos(0, 0, 0, nullptr);
    EXPECT_EQ(ret, 0);
}

/*
 * 测试用例名称 : ffrt_set_task_io_qos_fail
 * 测试用例描述：设置任务的IO QoS失败
 * 操作步骤    ：1、设置qos为-1
 * 预期结果    ：执行失败
 */
HWTEST_F(ResourceMngrTest, ffrt_set_task_io_qos_fail, TestSize.Level1)
{
    int ret = ffrt_set_task_io_qos(0, 0, -1, nullptr);
    EXPECT_EQ(ret, 1);
}