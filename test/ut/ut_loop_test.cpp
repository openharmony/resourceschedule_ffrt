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

#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include "ffrt_inner.h"
#include "c/loop.h"

using namespace std;
using namespace ffrt;
using namespace testing;

class LoopTest : public testing::Test {
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

void* ThreadFunc(void* p)
{
    int ret = ffrt_loop_run(p);
    EXPECT_EQ(ret, 0);
    return nullptr;
}

/*
 * 测试用例名称：loop_null_queue_create_fail
 * 测试用例描述：非法队列创建loop失败
 * 预置条件    ：无
 * 操作步骤    ：1、创建loop失败
 *
 * 预期结果    ：创建失败
 */
TEST_F(LoopTest, loop_null_queue_create_fail)
{
    auto loop = ffrt_loop_create(nullptr);
    EXPECT_EQ(loop, nullptr);
}

/*
 * 测试用例名称：loop_serial_queue_create_succ
 * 测试用例描述：serial队列创建loop失败
 * 预置条件    ：1、调用串行队列创建接口创建serial队列
 * 操作步骤    ：1、创建loop
 *
 * 预期结果    ：创建失败
 */
TEST_F(LoopTest, loop_serial_queue_create_succ)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    auto loop = ffrt_loop_create(queue_handle);
    EXPECT_EQ(loop, nullptr);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：loop_concurrent_queue_create_succ
 * 测试用例描述：无任务concurrent队列创建loop成功
 * 预置条件    ：1、调用串行队列创建接口创建concurrent队列
 * 操作步骤    ：1、创建loop
 *
 * 预期结果    ：执行成功
 */
TEST_F(LoopTest, loop_concurrent_queue_create_succ)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);

    auto loop = ffrt_loop_create(queue_handle);
    EXPECT_NE(loop, nullptr);

    int ret = ffrt_loop_destroy(loop);
    EXPECT_EQ(ret, 0);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：loop_concurrent_queue_create_fail
 * 测试用例描述：有任务队列创建loop失败
 * 预置条件    ：1、调用串行队列创建接口创建concurrent队列
 *            2、创建loop前向队列提交任务
 * 操作步骤    ：1、创建loop
 *
 * 预期结果    ：创建失败
 */
TEST_F(LoopTest, loop_concurrent_queue_create_fail)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);

    int result1 = 0;
    std::function<void()>&& basicFunc1 = [&result1]() { result1 += 10; };
    ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc1, ffrt_function_kind_queue), nullptr);

    auto loop = ffrt_loop_create(queue_handle);
    EXPECT_EQ(loop, nullptr);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：loop_run_fail
 * 测试用例描述：非法loop run失败
 * 操作步骤    ：1、执行loop run
 *
 * 预期结果    ：执行失败
 */
TEST_F(LoopTest, loop_run_fail)
{
    int ret = ffrt_loop_run(nullptr);
    EXPECT_NE(ret, 0);
}

/*
 * 测试用例名称：loop_destroy_fail
 * 测试用例描述：非法loop destroy失败
 * 操作步骤    ：1、执行loop run
 *
 * 预期结果    ：执行失败
 */
TEST_F(LoopTest, loop_destroy_fail)
{
    int ret = ffrt_loop_destroy(nullptr);
    EXPECT_NE(ret, 0);
}

/*
 * 测试用例名称：loop_run_destroy_success
 * 测试用例描述：正常loop run成功、destroy
 * 预置条件    ：1、调用串行队列创建接口创建concurrent队列
 *            2、用队列创建loop
 * 操作步骤    ：1、启动线程执行loop run
 *            2、销毁loop成功
 * 预期结果    ：执行成功
 */
TEST_F(LoopTest, loop_run_destroy_success)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);

    auto loop = ffrt_loop_create(queue_handle);
    EXPECT_NE(loop, nullptr);

    pthread_t thread;
    pthread_create(&thread, 0, ThreadFunc, loop);

    ffrt_loop_stop(loop);
    int ret = ffrt_loop_destroy(loop);
    EXPECT_EQ(ret, 0);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}