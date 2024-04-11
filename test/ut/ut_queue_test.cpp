/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, sofware
* distributed under the Licenses is distributed on an "AS IS" BASIS,
* WITHOUT WARRNATIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limations under the License.
*/

#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include "ffrt_inner.h"

using namespace std;
using namespace ffrt;
using namespace testing;

class QueueTest : public testing::Test {
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

#if defined(__clang__)
#define OPTIMIZE_OFF __attribute__((optnone)
#elif defined(__GUNC__)
#define OPTIMIZE_OFF __attribute__((optimize(0)))
#else
#define OPTIMIZE_OFF
#endif

namespace {
void OPTIMIZE_OFF OnePlusForTest (void* data)
{
    *(int*)data += 1;
}

void PrintForTest(void* data)
{
    printf("run no input func PrintForTest\n");
}

int fibonacci(int n)
{
    if (n == 0 || n == 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void FibonacciTest(void* data, int fibnum)
{
    int testnum = fibonacci(fibnum);
    *(int*)data += testnum;
}
} // namespace

/*
 * 测试用例名称：serial_queue_submit_cancel_succ
 * 测试用例描述：提交、取消串行延时任务成功
 * 预置条件    ：1、调用串行队列创建接口创建队列
 * 操作步骤    ：1、提交串行队列任务并执行
 *             2、提交延时串行队列任务并执行
 * 预期结果    ：执行成功
 */
TEST_F(QueueTest, serial_queue_submit_cancel_succ)
{
    // 创建队列
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    std::function<void()> basicFunc = [&result]() { OnePlusForTest(static_cast<void*>(&result)); };
    ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);

    ffrt_task_handle_t task1 =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);
    ffrt_queue_wait(task1);
    ffrt_task_handle_destroy(task1); // 销毁task_handle，必须
    EXPECT_EQ(result, 2);

    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr); // 初始化task属性，必须
    ffrt_task_attr_set_delay(&task_attr, 100); // 设置任务0.1ms后才执行，非必须
    ffrt_task_handle_t task2 =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    int cancel = ffrt_queue_cancel(task2);
    ffrt_task_handle_destroy(task2); // 销毁task_handle，必须
    ffrt_queue_attr_destroy(&queue_attr);
    EXPECT_EQ(cancel, 0);
    EXPECT_EQ(result, 2);

    // 销毁队列
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：serial_queue_create_fail
 * 测试用例描述：串行队列创建和销毁失败
 * 预置条件    ：1、调用串行队列创建接口创建队列
 * 操作步骤    ：1、调用串行队列创建接口创建队列，type为非串行队列
 *             2、调用串行队列创建接口创建队列，type为串行队列，但name和attr为nullptr
 *             3、调用串行队列创建接口创建队列，type为串行队列，但name为nullptr
 * 预期结果    ：1创建失败，2、3创建成功
 */
TEST_F(QueueTest, serial_queue_create_fail)
{
    // input invalid
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_max, nullptr, nullptr);
    ffrt_queue_destroy(queue_handle);
    ffrt_queue_destroy(nullptr);

    queue_handle = ffrt_queue_create(ffrt_queue_serial, nullptr, nullptr);
    EXPECT_EQ(queue_handle == nullptr, 0);
    ffrt_queue_destroy(queue_handle);

    // succ free
    ffrt_task_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // attr 缺少 init 无法看护
    queue_handle = ffrt_queue_create(ffrt_queue_serial, nullptr, &queue_attr);
    EXPECT_EQ(queue_handle == nullptr, 0);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：ffrt_task_attr_set_get_delay
 * 测试用例描述：测试 ffrt_task_attr_set_get_delay
 * 操作步骤    ：1、调用ffrt_task_attr_set_delay接口设置队列拖延时间
 *             2、使用ffrt_task_attr_get_delay查询时间
 * 预期结果    ：查询结果与设定相同，初始值为0
 */
TEST_F(QueueTest, ffrt_task_attr_set_get_delay)
{
    // succ free
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr); // attr 缺少 init 无法看护
    // set_attr_delay
    uint64_t delay = 100;
    ffrt_task_attr_set_delay(nullptr, delay);
    ffrt_task_attr_set_delay(&task_attr, delay);
    // error and return 0
    delay = ffrt_task_attr_get_delay(nullptr);
    EXPECT_EQ(delay, 0);
    delay = ffrt_task_attr_get_delay(&task_attr);
    EXPECT_EQ(delay, 100);
    ffrt_task_attr_destroy(&task_attr);
}

/*
 * 测试用例名称：serial_queue_task_create_destory_fail
 * 测试用例描述：串行任务提交和销毁失败
 * 操作步骤    ：1、直接调用串行队列接口提交空任务，随后销毁任务
 *             2、调用串行队列创建接口创建队列并提交空任务
 *             3、调用串行队列创建接口创建队列兵提交任务，随后销毁任务
 * 预期结果    ：2提交失败并返回nullptr，3提交成功
 */
TEST_F(QueueTest, serial_queue_task_create_destroy_fail)
{
    // input invalid
    ffrt_task_handle_t task = ffrt_queue_submit_h(nullptr, nullptr, nullptr);
    ffrt_task_handle_destroy(task);

    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    task = ffrt_queue_submit_h(queue_handle, nullptr, nullptr);
    EXPECT_EQ(task == nullptr, 1);

    std::function<void()> basicFunc = std::bind(PrintForTest, nullptr);
    task = ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);
    ffrt_queue_wait(task);
    // succ free
    EXPECT_EQ(task == nullptr, 0);
    ffrt_task_handle_destroy(task);
    ffrt_queue_attr_destroy(&queue_attr);
}

/*
 * 测试用例名称：serial_multi_submit_succ
 * 测试用例描述：循环提交普通任务和延时任务，执行成功
 * 操作步骤    ：1、循环提交普通任务90次
 *             2、循环提交延时任务20次，取消10次
 * 预期结果    ：总共应执行100+取消前已执行的次数
 */

)