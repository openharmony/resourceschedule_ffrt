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
#include "ffrt_inner.h"

using namespace std;
using namespace testing;

class DumpTest : public testing::Test {
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
 * 测试用例名称 : dump_succ
 * 测试用例描述：获取dump info失败
 * 操作步骤 ：1、调用函数ffrt_dump获取dump info
 * 预期结果 ：获取失败
 */
TEST_F(DumpTest, dump_succ)
{
    char dumpinfo[1024 * 512] = {0};
    int ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_INFO_ALL, dumpinfo, 1024 * 512);
    EXPECT_EQ(ret, -1);
}