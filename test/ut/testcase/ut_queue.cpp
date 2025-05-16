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
#include "c/queue_ext.h"
#include "../common.h"
#include "queue/base_queue.h"
#include "sync/delayed_worker.h"
#define private public
#include "queue/queue_monitor.h"
#undef private
#include "util/spmc_queue.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class QueueTest : public testing::Test {
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

void EmptyFunction() {}
/*
 * 测试用例名称 : serial_queue_submit_cancel_succ
 * 测试用例描述：提交、取消串行延时任务成功
 * 预置条件    ：1、调用串行队列创建接口创建队列
 * 操作步骤    ：1、提交串行队列任务并执行
 *              2、提交延时串行队列任务并执行
 * 预期结果    ：执行成功
 */
HWTEST_F(QueueTest, serial_queue_submit_cancel_succ, TestSize.Level1)
{
    // 创建队列
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    std::function<void()> basicFunc = [&result]() { OnePlusForTest(static_cast<void*>(&result)); };
    reinterpret_cast<ffrt::queue*>(&queue_handle)->submit(basicFunc);

    ffrt_task_handle_t task1 =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);
    ffrt_queue_wait(task1);
    ffrt_task_handle_destroy(task1); // 销毁task_handle，必须
    EXPECT_EQ(result, 2);

    ffrt::task_handle task2 =
        reinterpret_cast<ffrt::queue*>(&queue_handle)->submit_h(basicFunc, ffrt::task_attr().delay(1000));
    int cancel = reinterpret_cast<ffrt::queue*>(&queue_handle)->cancel(task2);
    ffrt_queue_attr_destroy(&queue_attr);
    EXPECT_EQ(cancel, 0);
    EXPECT_EQ(result, 2);

    // 销毁队列
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称 : serial_queue_create_fail
 * 测试用例描述：串行队列创建和销毁失败
 * 预置条件    ：1、调用串行队列创建接口创建队列
 * 操作步骤    ：1、调用串行队列创建接口创建队列，type为非串行队列
 *              2、调用串行队列创建接口创建队列，type为串行队列，但name与attr为nullptr
 *              3、调用串行队列创建接口创建队列，type为串行队列，但name为nullptr
 * 预期结果    ：1创建失败，2、3创建成功
 */
HWTEST_F(QueueTest, serial_queue_create_fail, TestSize.Level1)
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
 * 测试用例名称 : ffrt_task_attr_set_get_delay
 * 测试用例描述：测试 ffrt_task_attr_set_get_delay
 * 操作步骤    ：1、调用ffrt_task_attr_set_delay接口设置队列延时时间
 *              2、使用ffrt_task_attr_get_delay查询时间
 * 预期结果    ：查询结果与设定相同，初始值为0
 */
HWTEST_F(QueueTest, ffrt_task_attr_set_get_delay, TestSize.Level1)
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
 * 测试用例名称 : serial_queue_task_create_destroy_fail
 * 测试用例描述：串行任务提交和销毁失败
 * 操作步骤    ：1、直接调用串行队列接口提交空任务，随后销毁任务
 *              2、调用串行队列创建接口创建队列并提交空任务
 *              3、调用串行队列创建接口创建队列并提交任务，随后销毁任务
 * 预期结果    ：2提交失败并返回nullptr，3提交成功
 */
HWTEST_F(QueueTest, serial_queue_task_create_destroy_fail, TestSize.Level1)
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
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称 : serial_multi_submit_succ
 * 测试用例描述：循环提交普通任务和延时任务，执行成功
 * 操作步骤    ：1、循环提交普通任务90次
 *              2、循环提交延时任务20次，取消10次
 * 预期结果    ：总共应执行100+取消前已执行的次数
 */
HWTEST_F(QueueTest, serial_multi_submit_succ, TestSize.Level1)
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
 * 测试用例名称 : concurrent_multi_submit_succ
 * 测试用例描述：循环提交普通任务和延时任务，执行成功
 * 操作步骤    ：1、循环提交普通任务90次
 *              2、循环提交延时任务20次，取消10次
 * 预期结果    ：总共应执行100+取消前已执行的次数
 */
HWTEST_F(QueueTest, concurrent_multi_submit_succ, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);

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
 * 测试用例名称 : serial_early_quit_succ
 * 测试用例描述：主动销毁队列，未执行的任务取消
 * 操作步骤    ：1、提交10000个斐波那契任务
                2、至少取消1个
 * 预期结果    ：取消成功
 */
HWTEST_F(QueueTest, serial_early_quit_succ, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    int fibnum = 10;
    int result = 0;
    int expect = fibonacci(fibnum);
    std::function<void()>&& basicFunc = [&result, fibnum]() {
        FibonacciTest(static_cast<void*>(&result), fibnum);
        usleep(10);
    };
    for (int i = 0; i < 10000; ++i) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);
    }

    ffrt_queue_destroy(queue_handle);
    printf("result = %d\n", result);
    EXPECT_EQ(result < expect * 10000, 1);
}

/*
 * 测试用例名称 : serial_double_cancel_failed
 * 测试用例描述：对一个任务取消两次
 * 操作步骤    ：1、调用串行队列创建接口创建队列，设置延时并提交任务
                2、调用两次ffrt_queue_cancel取消同一任务
 * 预期结果    ：首次取消成功，第二次取消失败
 */
HWTEST_F(QueueTest, serial_double_cancel_failed, TestSize.Level1)
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
 * 测试用例名称 : ffrt_queue_attr_des
 * 测试用例描述：设置串行队列qos等级，销毁队列attr
 * 操作步骤    ：1、设置队列qos等级，调用串行队列创建接口创建队列
                2、调用ffrt_queue_attr_destroy接口销毁队列创建的attr
 * 预期结果    ：设置与销毁成功
 */
HWTEST_F(QueueTest, ffrt_queue_attr_des, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_attr_set_qos(&queue_attr, ffrt_qos_background);
    ffrt_qos_t qos = ffrt_queue_attr_get_qos(&queue_attr);
    EXPECT_EQ(qos == ffrt_qos_background, 1);
    ffrt_queue_attr_destroy(&queue_attr);
}

HWTEST_F(QueueTest, ffrt_queue_dfx_api_0001, TestSize.Level1)
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

HWTEST_F(QueueTest, ffrt_queue_dfx_api_0002, TestSize.Level1)
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

HWTEST_F(QueueTest, ffrt_queue_dfx_api_0004, TestSize.Level1)
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
 * 测试用例名称 : ffrt_task_attr_set_queue_priority
 * 测试用例描述 : 测试 ffrt_task_attr_set_queue_priority
 * 操作步骤     : 1、调用ffrt_task_attr_set_queue_priority接口设置队列优先级
 *               2、使用ffrt_task_attr_get_queue_priority查询优先级
 * 预期结果    : 查询结果与设定相同，值为3
 */
HWTEST_F(QueueTest, ffrt_task_attr_set_queue_priority, TestSize.Level1)
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
 * 测试用例名称 : ffrt_queue_attr_set_max_concurrency
 * 测试用例描述 : 测试 ffrt_queue_attr_set_max_concurrency
 * 操作步骤     : 1、调用ffrt_queue_attr_set_max_concurrency设置FFRT并行队列，并行度为4
 *               2、使用ffrt_queue_attr_get_max_concurrency查询并行度
 * 预期结果    : 查询结果与设定相同，值为4
 */
HWTEST_F(QueueTest, ffrt_queue_attr_set_max_concurrency, TestSize.Level1)
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
    concurrency = ffrt_queue_attr_get_max_concurrency(&queue_attr1);
    EXPECT_EQ(concurrency, 1);
    ffrt_queue_attr_destroy(&queue_attr1);
}

/*
 * 测试用例名称 : ffrt_queue_has_task
 * 测试用例描述 : 测试 ffrt_queue_has_task
 * 操作步骤     : 1、往队列中提交若干任务，其中包含待查询的任务
 *               2、调用ffrt_queue_has_task查询任务是否在队列中
 * 预期结果    : 查询结果与预期相同
 */
HWTEST_F(QueueTest, ffrt_queue_has_task, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    std::mutex lock;
    lock.lock();
    std::function<void()> basicFunc = [&]() { lock.lock(); };
    std::function<void()> emptyFunc = []() {};

    ffrt_task_attr_t task_attr;
    ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_name(&task_attr, "basic_function");
    ffrt_task_handle_t handle = ffrt_queue_submit_h(queue_handle,
        create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);

    for (int i = 0; i < 10; i++) {
        std::string name = "empty_function_" + std::to_string(i);
        ffrt_task_attr_set_name(&task_attr, name.c_str());
        ffrt_queue_submit(queue_handle, create_function_wrapper(emptyFunc, ffrt_function_kind_queue), &task_attr);
    }

    // 全字匹配
    for (int i = 0; i < 10; i++) {
        std::string name = "empty_function_" + std::to_string(i);
        bool hasEmptyTask = ffrt_queue_has_task(queue_handle, name.c_str());
        EXPECT_EQ(hasEmptyTask, true);
    }

    // 正则匹配
    bool hasEmptyTask = ffrt_queue_has_task(queue_handle, "empty_function_.*");
    EXPECT_EQ(hasEmptyTask, true);

    hasEmptyTask = ffrt_queue_has_task(queue_handle, "random_function");
    EXPECT_EQ(hasEmptyTask, false);

    lock.unlock();
    ffrt_queue_wait(handle);

    ffrt_task_handle_destroy(handle);
    ffrt_task_attr_destroy(&task_attr);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称 : ffrt_queue_cancel_all_and_cancel_by_name
 * 测试用例描述 : 测试 ffrt_queue_cancel_all、ffrt_queue_cancel_by_name
 * 操作步骤     : 1、往队列中提交若干任务
 *               2、调用ffrt_queue_cancel_by_name取消指定任务
 *               3、调用ffrt_queue_cancel_all取消所有任务
 * 预期结果    : 任务取消成功
 */
HWTEST_F(QueueTest, ffrt_queue_cancel_all_and_cancel_by_name, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(
        static_cast<ffrt_queue_type_t>(ffrt_queue_eventhandler_adapter), "test_queue", &queue_attr);

    std::mutex lock;
    lock.lock();
    std::function<void()> basicFunc = [&]() { lock.lock(); };
    std::function<void()> emptyFunc = []() {};

    ffrt_task_attr_t task_attr;
    ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_name(&task_attr, "basic_function");
    ffrt_task_handle_t handle = ffrt_queue_submit_h(queue_handle,
        create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);

    for (int i = 0; i < 10; i++) {
        std::string name = "empty_function_" + std::to_string(i);
        ffrt_task_attr_set_name(&task_attr, name.c_str());
        ffrt_queue_submit(queue_handle, create_function_wrapper(emptyFunc, ffrt_function_kind_queue), &task_attr);
    }

    // 测试ffrt_queue_cancel_by_name
    bool hasEmptyTask = ffrt_queue_has_task(queue_handle, "empty_function_3");
    EXPECT_EQ(hasEmptyTask, true);

    ffrt_queue_cancel_by_name(queue_handle, "empty_function_3");

    hasEmptyTask = ffrt_queue_has_task(queue_handle, "empty_function_3");
    EXPECT_EQ(hasEmptyTask, false);

    // 测试ffrt_queue_cancel_all
    hasEmptyTask = ffrt_queue_has_task(queue_handle, "empty_function_.*");
    EXPECT_EQ(hasEmptyTask, true);

    bool isIdle = ffrt_queue_is_idle(queue_handle);
    EXPECT_EQ(isIdle, false);

    ffrt_queue_cancel_all(queue_handle);

    hasEmptyTask = ffrt_queue_has_task(queue_handle, "empty_function_.*");
    EXPECT_EQ(hasEmptyTask, false);

    lock.unlock();
    ffrt_queue_cancel_and_wait(queue_handle);
    ffrt_queue_wait(handle);

    isIdle = ffrt_queue_is_idle(queue_handle);
    EXPECT_EQ(isIdle, true);

    ffrt_task_attr_destroy(&task_attr);
    ffrt_task_handle_destroy(handle);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称 : ffrt_queue_submit_head
 * 测试用例描述 : 测试 ffrt_queue_submit_head
 * 操作步骤     : 1、往队列中提交若干任务
 *               2、调用ffrt_queue_submit_head提交任务至队头
 * 预期结果    : 提交到对头的任务优先被执行
 */
HWTEST_F(QueueTest, ffrt_queue_submit_head, TestSize.Level1)
{
    ffrt::queue* testQueue = new ffrt::queue(static_cast<ffrt::queue_type>(
        ffrt_inner_queue_type_t::ffrt_queue_eventhandler_adapter), "test_queue");

    int result = 0;
    std::mutex lock;
    lock.lock();
    std::function<void()> basicFunc = [&]() { lock.lock(); };
    std::vector<std::function<void()>> assignFuncs(8, nullptr);
    std::vector<int> results;
    std::vector<int> expectResults {6, 2, 1, 4, 3, 5, 8, 7};
    for (int idx = 0; idx < 8; idx++) {
        assignFuncs[idx] = [idx, &results]() {
            results.push_back(idx + 1);
        };
    }

    ffrt::task_attr taskAttr;
    taskAttr.priority(ffrt_queue_priority_immediate).name("basic_function");
    testQueue->submit_head(basicFunc, taskAttr);
    testQueue->submit_head(assignFuncs[0], taskAttr);
    testQueue->submit_head(assignFuncs[1], taskAttr);

    taskAttr.priority(ffrt_queue_priority_high);
    testQueue->submit_head(assignFuncs[2], taskAttr);
    testQueue->submit_head(assignFuncs[3], taskAttr);

    taskAttr.priority(ffrt_queue_priority_low);
    testQueue->submit_head(assignFuncs[4], taskAttr);

    taskAttr.priority(ffrt_queue_priority_immediate);
    testQueue->submit_head(assignFuncs[5], taskAttr);

    taskAttr.priority(ffrt_queue_priority_idle);
    ffrt::task_handle handle = testQueue->submit_head_h(assignFuncs[6], taskAttr);
    testQueue->submit_head(assignFuncs[7], taskAttr);

    lock.unlock();
    testQueue->wait(handle);
    EXPECT_EQ(results, expectResults);
    delete testQueue;
}

HWTEST_F(QueueTest, ffrt_get_main_queue, TestSize.Level1)
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
    delete serialQueue;
}

HWTEST_F(QueueTest, ffrt_get_current_queue, TestSize.Level1)
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
            EXPECT_GT(ffrt::get_queue_id(), 0);
            result = result + 1;
            ffrt_queue_submit(currentQueue, ffrt::create_function_wrapper(basicFunc, ffrt_function_kind_queue),
                              &attr);
        },
        ffrt::task_attr().qos(3).name("ffrt current_queue."));

    serialQueue->wait(handle);

    EXPECT_EQ(result, 1);
    delete serialQueue;
}

/*
 * 测试用例名称 : ffrt_queue_recordtraffic_normal_trigger
 * 测试用例描述 : 设置串行队列的traffic_interval并生效
 * 操作步骤     : 1、创建队列
 *               2、提交堆积任务
 * 预期结果    : 成功触发流量监控告警
 */
HWTEST_F(QueueTest, ffrt_queue_recordtraffic_normal_trigger, TestSize.Level1)
{
    ffrt::DelayedWorker::GetInstance();
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 30000000;
    ffrt_queue_attr_t queue_attr;
    ffrt_task_handle_t handle;
    ffrt_task_attr_t task_attr;
    int result = 0;
    (void)ffrt_task_attr_init(&task_attr);
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    ffrt::QueueHandler* queueHandler = reinterpret_cast<ffrt::QueueHandler*>(queue_handle);
    queueHandler->trafficRecordInterval_ = 1000000;
    queueHandler->trafficRecord_.nextUpdateTime_ = TimeStampCntvct() + 1000000;

    std::function<void()>&& firstFunc = [&result]() {
        result = result + 1;
        usleep(1100000);
    };
    std::function<void()>&& fastFunc = [&result]() {
        result = result + 1;
    };

    ffrt_queue_submit(queue_handle, create_function_wrapper(firstFunc, ffrt_function_kind_queue), &task_attr);
    for (int i = 0; i < 30; i++) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr);
    }
    usleep(1000000);
    handle = ffrt_queue_submit_h(queue_handle, create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr);
    ffrt_queue_wait(handle);
    EXPECT_EQ(result, 32);
    ffrt_task_handle_destroy(handle);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
* 测试用例名称 : ffrt_queue_recordtraffic_normal_corner
* 测试用例描述 : 串行队列的traffic_interval并生效
* 操作步骤     : 1、创建队列
*               2、提交堆积任务
* 预期结果    : 不触发流量监控告警
*/
HWTEST_F(QueueTest, ffrt_queue_recordtraffic_normal_corner, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    const int numTasks = 19;
    ffrt_task_handle_t handle[numTasks];
    ffrt_task_attr_t task_attr;
    int result = 0;
    (void)ffrt_task_attr_init(&task_attr);
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    std::function<void()>&& firstFunc = [&result]() {
        result = result + 1;
        usleep(1100000);
    };
    std::function<void()>&& fastFunc = [&result]() {
        result = result + 1;
    };

    ffrt_queue_submit(queue_handle, create_function_wrapper(firstFunc, ffrt_function_kind_queue), &task_attr);
    for (int i = 0; i < numTasks; i++) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr);
    }
    usleep(1000000);
    ffrt_queue_submit(queue_handle, create_function_wrapper(firstFunc, ffrt_function_kind_queue), &task_attr);
    for (int i = 0; i < numTasks; i++) {
        handle[i] = ffrt_queue_submit_h(queue_handle, create_function_wrapper(fastFunc,
            ffrt_function_kind_queue), &task_attr);
    }
    ffrt_queue_wait(handle[numTasks -1]);

    EXPECT_EQ(result, 40);
    for (int i = 0; i < numTasks; i++) {
        ffrt_task_handle_destroy(handle[i]);
    }
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称 : ffrt_queue_recordtraffic_delay_trigger
 * 测试用例描述 : 设置串行队列的traffic_interval并生效
 * 操作步骤     : 1、创建队列
 *               2、提交堆积任务
 * 预期结果    : 成功触发流量监控告警，但不触发上报
 */
HWTEST_F(QueueTest, ffrt_queue_recordtraffic_delay_trigger, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    ffrt_task_handle_t handle;
    ffrt_task_attr_t task_attr;
    ffrt_task_attr_t task_attr1;
    int result = 0;
    (void)ffrt_task_attr_init(&task_attr1);
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_delay(&task_attr, 1200000);
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    std::function<void()>&& firstFunc = [&result]() {
        result = result + 1;
        usleep(1100000);
    };
    std::function<void()>&& fastFunc = [&result]() {
        result = result + 1;
    };

    ffrt_queue_submit(queue_handle, create_function_wrapper(firstFunc, ffrt_function_kind_queue), &task_attr1);
    for (int i = 0; i < 30; i++) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr);
    }
    usleep(1000000);
    handle = ffrt_queue_submit_h(queue_handle, create_function_wrapper(fastFunc,
        ffrt_function_kind_queue), &task_attr1);
    ffrt_queue_wait(handle);
    EXPECT_EQ(result, 2);
    ffrt_task_handle_destroy(handle);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

void MyCallback(uint64_t id, const char* message, uint32_t length)
{
    FFRT_LOGE("call ffrt_queue_monitor timeout_callback");
}

/*
 * 测试用例名称 : ffrt_queue_monitor_schedule_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个任务占满worker，使得新串行任务等待调度
 * 预期结果    : 成功触发任务调度超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_schedule_timeout111, TestSize.Level1)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt::DelayedWorker::GetInstance();
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 1000000;

    for (int i = 0; i < 16; i++) {
        ffrt::submit([&x]() {
            x = x + 1;
            usleep(1100000);
        }, {}, {});
    }
    queue* testQueue = new queue("test_queue");

    auto t = testQueue->submit_h([] {
        FFRT_LOGE("task start");}, {});
    testQueue->wait(t);
    delete testQueue;
    EXPECT_EQ(x, 16);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_execute_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 成功触发任务执行超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_execute_timeout, TestSize.Level1)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt::DelayedWorker::GetInstance();
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 1000000;
    queue* testQueue = new queue("test_queue");
    auto t = testQueue->submit_h([&x] { x = x + 1; usleep(1100000); FFRT_LOGE("done");}, {});
    FFRT_LOGE("submitted");
    testQueue->wait(t);
    delete testQueue;
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_delay_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 不触发超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_delay_timeout, TestSize.Level1)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt::DelayedWorker::GetInstance();
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 1000000;
    queue* testQueue = new queue("test_queue");
    FFRT_LOGE("submit");
    auto t = testQueue->submit_h([&x] { x = x + 1; FFRT_LOGE("delay start"); }, task_attr().delay(1500000));
    testQueue->wait(t);
    delete testQueue;
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_cancel_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 不触发超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_cancel_timeout, TestSize.Level1)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt::DelayedWorker::GetInstance();
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 1000000;
    queue* testQueue = new queue("test_queue");
    FFRT_LOGE("submit");
    testQueue->submit([&x] { x = x + 1; FFRT_LOGE("start"); });
    auto t = testQueue->submit_h([&x] { x = x + 1; FFRT_LOGE("delay start"); }, task_attr().delay(5000000));
    testQueue->cancel(t);
    testQueue->wait(t);
    usleep(1200000);
    delete testQueue;
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

static inline void StallUsImpl(size_t us)
{
    auto start = std::chrono::system_clock::now();
    size_t passed = 0;
    while (passed < us) {
        passed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - start).count();
    }
}

void StallUs(size_t us)
{
    StallUsImpl(us);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_two_stage_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务在PENDING和EXECUTING各超时一次
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 触发PENDING和EXECUTING各一次的超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_two_stage_timeout, TestSize.Level1)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt::DelayedWorker::GetInstance();
    ffrt::QueueMonitor::GetInstance().timeoutUs_ = 1000000;

    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 1);

    ffrt_queue_attr_t queue_attr;
    ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    ffrt::submit([] { StallUs(1300 * 1000); });
    std::function<void()>&& basicFunc = [&x] { x = x + 1; StallUs(1300 * 1000); FFRT_LOGE("done");};
    ffrt_task_handle_t task = ffrt_queue_submit_h(queue_handle,
        ffrt::create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);

    ffrt_queue_wait(task);
    ffrt_task_handle_destroy(task);
    ffrt_queue_destroy(queue_handle);
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_spmc_queue_test
 * 测试用例描述 : SPMC无锁队列功能测试
 * 预置条件     ：创建一个SPMC队列
 * 操作步骤     : 1、调用PushTail接口向队列中push若干数据
 *               2、创建多个线程，并发地调用PopHead接口从队列中获取数据
 * 预期结果    : 数据能够被正确取出
 */
HWTEST_F(QueueTest, ffrt_spmc_queue_test, TestSize.Level1)
{
    SpmcQueue queue;
    EXPECT_EQ(queue.Init(0), -1);
    EXPECT_EQ(queue.PushTail(nullptr), -1);
    EXPECT_EQ(queue.PopHead(), nullptr);
    EXPECT_EQ(queue.Init(128), 0);

    int data[128];
    for (int i = 0; i < 128; i++) {
        data[i] = i;
        queue.PushTail(&data[i]);
    }
    EXPECT_EQ(queue.PushTail(&data[0]), -1);

    std::atomic<int> count = 0;
    std::vector<std::thread> consumers;
    for (int i = 0; i < 16; i++) {
        consumers.emplace_back([&queue, &count] {
            int* ret = reinterpret_cast<int*>(queue.PopHead());
            while (ret != nullptr) {
                count++;
                ret = reinterpret_cast<int*>(queue.PopHead());
            }
        });
    }

    for (auto& consumer : consumers) {
        consumer.join();
    }
    EXPECT_EQ(count.load(), 128);
}

/*
 * 测试用例名称 : ffrt_spmc_queue_pop_head_to_another_queue
 * 测试用例描述 : SPMC无锁队列数据迁移功能测试
 * 预置条件     ：创建一个SPMC源队列，创建一个SPMC目标队列，目标队列容量小于源队列
 * 操作步骤     : 1、调用PushTail接口向源队列中push若干数据
 *               2、调用PopHeadToAnotherQueue接口向目标队列中迁移小于源队列和目标队列容量的数据
 *               3、调用PopHeadToAnotherQueue接口向目标队列中迁移等于目标队列容量的数据
 *               4、调用PopHeadToAnotherQueue接口向目标队列中迁移超过源队列容量的数据
 * 预期结果    : 1、迁移成功，目标队列中存在和迁移数量相同的数据
 *              2、迁移成功，目标队列中存在和目标队列数量相同的数据
 *              3、迁移成功，目标队列中存在和源队列数量相同的数据
 */
HWTEST_F(QueueTest, ffrt_spmc_queue_pop_head_to_another_queue, TestSize.Level1)
{
    SpmcQueue queue;
    SpmcQueue dstQueue;
    SpmcQueue dstQueue2;
    EXPECT_EQ(queue.Init(128), 0);
    EXPECT_EQ(dstQueue.Init(64), 0);
    EXPECT_EQ(dstQueue2.Init(128), 0);

    int data[128];
    for (int i = 0; i < 128; i++) {
        data[i] = i;
        queue.PushTail(&data[i]);
    }

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue, 0, 0, nullptr), 0);
    EXPECT_EQ(dstQueue.GetLength(), 0);

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue, 32, 0, nullptr), 32);
    EXPECT_EQ(dstQueue.GetLength(), 32);

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue, 64, 0, [] (void* data, int qos) { EXPECT_NE(data, nullptr); }), 32);
    EXPECT_EQ(dstQueue.GetLength(), 64);

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue2, 128, 0, nullptr), 64);
    EXPECT_EQ(dstQueue2.GetLength(), 64);
}

HWTEST_F(QueueTest, ffrt_queue_submit_h_f, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    ffrt_task_handle_t task = ffrt_queue_submit_h_f(queue_handle, OnePlusForTest, &result, nullptr);
    ffrt_queue_wait(task);
    ffrt_task_handle_destroy(task);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);

    EXPECT_EQ(result, 1);
}

HWTEST_F(QueueTest, ffrt_queue_submit_f, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    ffrt_queue_submit_f(queue_handle, OnePlusForTest, &result, nullptr);
    EXPECT_TRUE(queue_handle != nullptr);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);

    EXPECT_EQ(result, 1);
}