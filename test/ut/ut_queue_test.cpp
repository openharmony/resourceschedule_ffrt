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
#define OPTIMIZE_OFF __attribute__((optnone))
#elif defined(__GNUC__)
#define OPTIMIZE_OFF __attribute__((optimize(0)))
#else
#define OPTIMIZE_OFF
#endif

namespace {
void OPTIMIZE_OFF OnePlusForTest(void* data)
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
 * 测试用例名称 ：serial_queue_submit_cancel_succ
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
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // attr 缺少 init 无法看护
    queue_handle = ffrt_queue_create(ffrt_queue_serial, nullptr, &queue_attr);
    EXPECT_EQ(queue_handle == nullptr, 0);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：ffrt_task_attr_set_get_delay
 * 测试用例描述：测试 ffrt_task_attr_set_get_delay
 * 操作步骤    ：1、调用ffrt_task_attr_set_delay接口设置队列延时时间
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
 * 测试用例名称：serial_queue_task_create_destroy_fail
 * 测试用例描述：串行任务提交和销毁失败
 * 操作步骤    ：1、直接调用串行队列接口提交空任务，随后销毁任务
 *             2、调用串行队列创建接口创建队列并提交空任务
 *             3、调用串行队列创建接口创建队列并提交任务，随后销毁任务
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
TEST_F(QueueTest, serial_multi_submit_succ)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    int cancelFailedNum = 0;
    std::function<void()>&& basicFunc = [&result]() { OnePlusForTest(static_cast<void*>(&result)); };
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr); // 初始化task属性，必须
    ffrt_task_attr_set_delay(&task_attr, 100); // 设置任务0.1ms后才执行，非必须

    for (int n = 0; n < 10; ++n) {
        for (int i = 0; i < 9; ++i) {
            ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);
        }

        ffrt_task_handle_t t1 =
            ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
        ffrt_task_handle_t t2 =
            ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
        cancelFailedNum += ffrt_queue_cancel(t1);
        ffrt_task_handle_destroy(t1); // 销毁task_handle，必须

        ffrt_queue_wait(t2);
        ffrt_task_handle_destroy(t2);
    }

    EXPECT_EQ(result, (cancelFailedNum + 100));
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：serial_early_quit_succ
 * 测试用例描述：主动销毁队列，未执行的任务取消
 * 操作步骤    ：1、提交10000个斐波那契任务
 *             2、至少取消1个
 * 预期结果    ：取消成功
 */
TEST_F(QueueTest, serial_early_quit_succ)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    int fibnum = 10;
    int result = 0;
    int expect = fibonacci(fibnum);
    std::function<void()>&& basicFunc = [&result, fibnum]() { FibonacciTest(static_cast<void*>(&result), fibnum); };
    for (int i = 0; i < 10000; ++i) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);
    }

    ffrt_queue_destroy(queue_handle);
    printf("result = %d\n", result);
    EXPECT_EQ(result < expect * 10000, 1);
}

/*
 * 测试用例名称：serial_double_cancel_failed
 * 测试用例描述：对一个任务取消两次
 * 操作步骤    ：1、调用串行队列创建接口创建队列，设置延时并提交任务
 *             2、调用两次ffrt_queue_cancel取消同一任务
 * 预期结果    ：首次取消成功，第二次取消失败
 */
TEST_F(QueueTest, serial_double_cancel_failed)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    std::function<void()>&& basicFunc = [&result]() { OnePlusForTest(static_cast<void*>(&result)); };
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr); // 初始化task属性，必须
    ffrt_task_attr_set_delay(&task_attr, 100); // 设置任务0.1ms后才执行，非必须

    ffrt_task_handle_t t1 =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    int cancel = ffrt_queue_cancel(t1);
    EXPECT_EQ(cancel, 0);
    cancel = ffrt_queue_cancel(t1);
    EXPECT_EQ(cancel, 1);
    ffrt_task_handle_destroy(t1); // 销毁task_handle，必须

    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称：ffrt_queue_attr_des
 * 测试用例描述：设置串行队列qos等级，销毁队列attr
 * 操作步骤    ：1、设置队列qos等级，调用串行队列创建接口创建队列
 *             2、调用ffrt_queue_attr_destroy接口销毁队列创建的attr
 * 预期结果    ：设置与销毁成功
 */
TEST_F(QueueTest, ffrt_queue_attr_des)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_attr_set_qos(&queue_attr, ffrt_qos_background);
    ffrt_qos_t qos = ffrt_queue_attr_get_qos(&queue_attr);
    EXPECT_EQ(qos == ffrt_qos_background, 1);
    ffrt_queue_attr_destroy(&queue_attr);
}

/*
 * 测试用例名称：ffrt_queue_delay_timeout
 * 测试用例描述：任务队列超时，以及延时任务
 * 操作步骤    ：1、设置队列超时时间与超时回调，调用串行队列创建接口创建队列
 *             2、设置延时并提交任务
 * 预期结果    ：超时执行回调
 */
TEST_F(QueueTest, ffrt_queue_delay_timeout)
{
    int x = 0;
    std::function<void()>&& basicFunc1 = [&]() {
        x = x + 1;
    };
    ffrt_function_header_t* ffrt_header_t = ffrt::create_function_wrapper((basicFunc1));

    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_attr_set_callback(&queue_attr, ffrt_header_t);
    ffrt_queue_attr_set_timeout(&queue_attr, 2000);
    uint64_t timeout = ffrt_queue_attr_get_timeout(&queue_attr);
    EXPECT_EQ(timeout, 2000);
    
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    std::function<void()>&& basicFunc = [&result]() {
        OnePlusForTest(static_cast<void*>(&result));
        usleep(3000);
    };
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_delay(&task_attr, 1000);

    ffrt_task_handle_t t1 =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    
    ffrt_queue_wait(t1);
    ffrt_task_handle_destroy(t1);
    EXPECT_EQ(result, 1);
    EXPECT_EQ(x, 1);
    ffrt_queue_destroy(queue_handle);
}

TEST_F(QueueTest, ffrt_queue_dfx_api_0001)
{
    // ffrt_queue_attr_set_timeout接口attr为异常值
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_attr_set_timeout(nullptr, 10000);
    uint64_t time = ffrt_queue_attr_get_timeout(&queue_attr);
    EXPECT_EQ(time, 0);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    EXPECT_TRUE(queue_handle != nullptr);

    // 销毁队列
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

TEST_F(QueueTest, ffrt_queue_dfx_api_0002)
{
    // ffrt_queue_attr_get_timeout接口attr为异常值
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_attr_set_timeout(&queue_attr, 10000);
    uint64_t time = ffrt_queue_attr_get_timeout(nullptr);
    EXPECT_EQ(time, 0);
    time = ffrt_queue_attr_get_timeout(&queue_attr);
    EXPECT_EQ(time, 10000);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    EXPECT_TRUE(queue_handle != nullptr);

    // 销毁队列
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

TEST_F(QueueTest, ffrt_queue_dfx_api_0003)
{
    // ffrt_queue_attr_set_timeoutCb接口attr为异常值
    std::function<void()> cbOne = []() { printf("first set callback\n"); };

    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_attr_set_callback(nullptr, ffrt::create_function_wrapper(cbOne, ffrt_function_kind_queue));
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    EXPECT_TRUE(queue_handle != nullptr);

    // 销毁队列
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

TEST_F(QueueTest, ffrt_queue_dfx_api_0004)
{
    // ffrt_queue_attr_get_timeoutCb接口attr为异常值
    std::function<void()> cbOne = []() { printf("first set callback\n"); };

    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_attr_set_callback(&queue_attr, ffrt::create_function_wrapper(cbOne, ffrt_function_kind_queue));
    ffrt_function_header_t* func = ffrt_queue_attr_get_callback(nullptr);
    EXPECT_TRUE(func == nullptr);
    func = ffrt_queue_attr_get_callback(&queue_attr);
    EXPECT_TRUE(func != nullptr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    EXPECT_TRUE(queue_handle != nullptr);

    // 销毁队列
    ffrt_queue_destroy(queue_handle);
    ffrt_queue_attr_destroy(&queue_attr);
}

/*
 * 测试用例名称：ffrt_task_attr_set_queue_priority
 * 测试用例描述：测试ffrt_task_attr_set_queue_priority
 * 操作步骤    ：1、调用ffrt_task_attr_set_queue_priority接口设置队列优先级
 *             2、使用ffrt_task_attr_get_queue_priority查询优先级
 * 预期结果    ：查询结果与设定相同，值为3
 */
TEST_F(QueueTest, ffrt_task_attr_set_queue_priority)
{
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_queue_priority_t priority = ffrt_queue_priority_low;
    ffrt_task_attr_set_queue_priority(nullptr, priority);
    ffrt_task_attr_set_queue_priority(&task_attr, priority);
    priority = ffrt_task_attr_get_queue_priority(nullptr);
    EXPECT_EQ(priority, ffrt_queue_priority_immediate);
    priority = ffrt_task_attr_get_queue_priority(&task_attr);
    EXPECT_EQ(priority, ffrt_queue_priority_low);
    ffrt_task_attr_destroy(&task_attr);
}

/*
 * 测试用例名称：ffrt_queue_attr_set_max_concurrency
 * 测试用例描述：测试ffrt_queue_attr_set_max_concurrency
 * 操作步骤    ：1、调用ffrt_queue_attr_set_max_concurrency设置FFRT并行队列，并行度为4
 *             2、使用ffrt_queue_attr_get_max_concurrency查询并行度
 * 预期结果    ：查询结果与设定相同，值为4
 */
TEST_F(QueueTest, ffrt_queue_attr_set_max_concurrency)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    uint64_t concurrency = 4;
    ffrt_queue_attr_set_max_concurrency(nullptr, concurrency);
    ffrt_queue_attr_set_max_concurrency(&queue_attr, concurrency);
    concurrency = ffrt_queue_attr_get_max_concurrency(nullptr);
    EXPECT_EQ(concurrency, 0);
    concurrency = ffrt_queue_attr_get_max_concurrency(&queue_attr);
    EXPECT_EQ(concurrency, 4);
    ffrt_queue_attr_destroy(&queue_attr);

    ffrt_queue_attr_t queue_attr1;
    (void)ffrt_queue_attr_init(&queue_attr1);
    concurrency = 0;
    ffrt_queue_attr_set_max_concurrency(&queue_attr1, concurrency);
    concurrency = ffrt_queue_attr_get_max_concurrency(queue_attr1);
    EXPECT_EQ(concurrency, 1);
    ffrt_queue_attr_destroy(&queue_attr1);
}

#ifdef OHOS_STANDARD_SYSTEM
TEST_F(QueueTest, ffrt_get_main_queue)
{
 // ffrt test case begin
    ffrt::queue *serialQueue = new ffrt::queue("ffrt_normal_queue", {});
    ffrt_queue_t mainQueue = ffrt_get_main_queue();
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_qos(&attr, ffrt_qos_user_initiated);
    int result = 0;
    std::function<void()>&& basicFunc = [&result]() {
        OnePlusForTest(static_cast<void*>(&result));
        OnePlusForTest(static_cast<void*>(&result));
        EXPECT_EQ(result, 2);
        usleep(3000);
    };

    ffrt::task_handle handle = serialQueue->submit_h(
        [&] {
            result = result + 1;
            ffrt_queue_submit(mainQueue, ffrt::create_function_wrapper(basicFunc, ffrt_function_kind_queue),
                              &attr);
        },
        ffrt::task_attr().qos(3).name("ffrt main_queue."));

    serialQueue->wait(handle);
    EXPECT_EQ(result, 1);
}

TEST_F(QueueTest, ffrt_get_current_queue)
{
 // ffrt test case begin
    ffrt::queue *serialQueue = new ffrt::queue("ffrt_normal_queue", {});
    ffrt_queue_t currentQueue = ffrt_get_current_queue();
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_qos(&attr, ffrt_qos_user_initiated);
    int result = 0;
    std::function<void()>&& basicFunc = [&result]() {
        OnePlusForTest(static_cast<void*>(&result));
        OnePlusForTest(static_cast<void*>(&result));
        EXPECT_EQ(result, 3);
        usleep(3000);
    };

    ffrt::task_handle handle = serialQueue->submit_h(
        [&] {
            result = result + 1;
            ffrt_queue_submit(currentQueue, ffrt::create_function_wrapper(basicFunc, ffrt_function_kind_queue),
                              &attr);
        },
        ffrt::task_attr().qos(3).name("ffrt current_queue."));

    serialQueue->wait(handle);

    EXPECT_EQ(result, 1);
}
#endif