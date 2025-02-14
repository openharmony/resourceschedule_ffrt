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
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

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
 * 测试用例名称：dump_succ
 * 测试用例描述：获取dump info失败
 * 操作步骤    ：1、调用函数ffrt_dump获取dump info
 * 预期结果    ：获取失败
 */
HWTEST_F(DumpTest, dump_succ, TestSize.Level1)
{
    char dumpinfo[1024 * 512] = {0};
    int ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_INFO_ALL, dumpinfo, 1024 * 512);
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    EXPECT_NE(ret, -1);
#else
    EXPECT_EQ(ret, -1);
#endif
}

/*
 * 测试用例名称：dump_cb_succ
 * 测试用例描述：设置与获取task_timeout_cb成功
 * 预期结果    ：设置与获取成功
 */
HWTEST_F(DumpTest, dump_cb_succ, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(nullptr);
    ffrt_task_timeout_cb ret = ffrt_task_timeout_get_cb();
    EXPECT_EQ(ret, nullptr);
}

/*
 * 测试用例名称：dump_threshold_succ
 * 测试用例描述：设置与获取task_timeout_threshold成功
 * 预期结果    ：设置与获取成功
 */
HWTEST_F(DumpTest, dump_thre_succ, TestSize.Level1)
{
    ffrt_task_timeout_set_threshold(500);
    uint32_t ret = ffrt_task_timeout_get_threshold();
    EXPECT_EQ(ret, 500);
}

/*
 * 测试用例名称：dump_stat_succ
 * 测试用例描述：获取dump stat info成功
 * 操作步骤   ：1.调用函数ffrt_dump获取dump stat
 * 预期结果    ：获取成功
 */
HWTEST_F(DumpTest, dump_stat_succ, TestSize.Level1)
{
    char dumpinfo[1024 * 512] = {0};
    int ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_START_STAT, dumpinfo, 1024 * 512);
    EXPECT_EQ(ret, 0);
    ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_STOP_STAT, dumpinfo, 1024 * 512);
    EXPECT_EQ(ret, 0);
}

/*
 * 测试用例名称：dump_stat_succ
 * 测试用例描述：获取dump stat info失败
 * 操作步骤   ：1.调用函数ffrt_dump获取dump stat
 * 预期结果    ：获取失败
 */
HWTEST_F(DumpTest, dump_stat_fail, TestSize.Level1)
{
    int ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_START_STAT, nullptr, 1000);
    EXPECT_EQ(ret, -1);
    char dumpinfo[1024 * 512] = {0};
    ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_START_STAT, dumpinfo, 20);
    EXPECT_EQ(ret, -1);
    char dumpinfoTwo[1024 * 512] = {0};
    ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_START_STAT, dumpinfo, 1024 * 512);
    EXPECT_NE(ret, -1);
    ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_START_STAT, dumpinfoTwo, 1024 * 512);
    EXPECT_EQ(ret, -1);
    ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_STOP_STAT, dumpinfo, 1024 * 512);
    EXPECT_NE(ret, -1);
    ret = ffrt_dump(ffrt_dump_cmd_t::DUMP_STOP_STAT, dumpinfo, 1024 * 512);
    EXPECT_EQ(ret, -1);
}

/*
 * 测试用例名称：dump_task_stat_use_nullptr
 * 测试用例描述：dump接口传入空buf
 * 预期结果    ：设置与获取失败
 */
HWTEST_F(DumpTest, dump_task_stat_use_nullptr, TestSize.Level1)
{
    int bufferSize = 200;
    int result = ffrt_dump(DUMP_START_STAT, nullptr, bufferSize);
    EXPECT_EQ(result, -1);
}

/*
 * 测试用例名称：dump_task_stat_stop_invalid_addr
 * 测试用例描述：dump stop接口传入错误buf
 * 预期结果    ：设置与获取失败
 */
HWTEST_F(DumpTest, dump_task_stat_stop_invalid_addr, TestSize.Level1)
{
    char dumpinfo[1024 * 512] = {0};
    char dumpinfo_1[1024] = {0};
    int result = ffrt_dump(DUMP_START_STAT, dumpinfo, 1024 * 512);
    result = ffrt_dump(DUMP_STOP_STAT, dumpinfo_1, 1024);
    EXPECT_EQ(result, -1);
    result = ffrt_dump(DUMP_STOP_STAT, dumpinfo, 1024 * 512);
    EXPECT_EQ(result, 0);
}