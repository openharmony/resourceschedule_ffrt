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
#include "util/ffrt_facade.h"
#include "util/white_list.h"

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
 * 测试用例名称 : serial_queue_create_fail
 * 测试用例描述：串行队列创建和销毁失败
 * 预置条件    ：1、调用串行队列创建接口创建队列
 * 操作步骤    ：1、调用串行队列创建接口创建队列，type为非串行队列
 *              2、调用串行队列创建接口创建队列，type为串行队列，但name与attr为nullptr
 *              3、调用串行队列创建接口创建队列，type为串行队列，但name为nullptr
 * 预期结果    ：1创建失败，2、3创建成功
 */
HWTEST_F(QueueTest, serial_queue_create_fail, TestSize.Level0)
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
HWTEST_F(QueueTest, ffrt_task_attr_set_get_delay, TestSize.Level0)
{
    // succ free
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr); // attr 缺少 init 无法看护
    uint64_t maxUsCount = 1000000ULL * 100 * 60 * 60 * 24 * 365; // 100 year
    ffrt_task_attr_set_delay(&task_attr, UINT64_MAX); // 测试时间溢出截断功能
    EXPECT_EQ(ffrt_task_attr_get_delay(&task_attr), maxUsCount);
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
HWTEST_F(QueueTest, serial_queue_task_create_destroy_fail, TestSize.Level0)
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
HWTEST_F(QueueTest, serial_multi_submit_succ, TestSize.Level0)
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
HWTEST_F(QueueTest, concurrent_multi_submit_succ, TestSize.Level0)
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
HWTEST_F(QueueTest, serial_early_quit_succ, TestSize.Level0)
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
HWTEST_F(QueueTest, serial_double_cancel_failed, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    int result = 0;
    std::function<void()>&& basicFunc = [&result]() { OnePlusForTest(static_cast<void*>(&result)); };
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr); // 初始化task属性，必须
    constexpr uint64_t delayMs = 1000 * 100; // 100 milliseconds delay
    // we delay the task to make sure it does not complete before the first cancel.
    ffrt_task_attr_set_delay(&task_attr, delayMs);

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
HWTEST_F(QueueTest, ffrt_queue_attr_des, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_attr_set_qos(&queue_attr, ffrt_qos_background);
    ffrt_qos_t qos = ffrt_queue_attr_get_qos(&queue_attr);
    EXPECT_EQ(qos == ffrt_qos_background, 1);
    ffrt_queue_attr_destroy(&queue_attr);
}

HWTEST_F(QueueTest, ffrt_queue_dfx_timeout, TestSize.Level0)
{
    // ffrt_queue_attr_set_timeout接口attr为异常值
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_attr_set_timeout(nullptr, 10000);
    uint64_t time = ffrt_queue_attr_get_timeout(&queue_attr);
    EXPECT_EQ(time, 0);
    ffrt_queue_attr_set_timeout(&queue_attr, UINT64_MAX); // 测试时间溢出截断功能
    uint64_t maxUsCount = 1000000ULL * 100 * 60 * 60 * 24 * 365; // 100 year
    EXPECT_EQ(ffrt_queue_attr_get_timeout(&queue_attr), maxUsCount);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
    EXPECT_TRUE(queue_handle != nullptr);

    // 销毁队列
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

HWTEST_F(QueueTest, ffrt_queue_dfx_callback, TestSize.Level0)
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

HWTEST_F(QueueTest, ffrt_queue_dfx_api_0004, TestSize.Level0)
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
 * 测试用例名称 : get_queue_id_from_task
 * 测试用例描述 : 从队列任务中读取queueid
 * 操作步骤     : 1、创建串行队列，提交任务
 *               2、在任务中读取queueid
 * 预期结果    : 读取queueid成功
 */
HWTEST_F(QueueTest, get_queue_id_from_task, TestSize.Level0)
{
    int x = 0;
    auto testQueue = std::make_unique<queue>("test_queue");
    auto t = testQueue->submit_h([&] {
        x++;
        (void)ffrt::get_queue_id();
    }, {});
    testQueue->wait(t);
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_task_attr_set_queue_priority
 * 测试用例描述 : 测试 ffrt_task_attr_set_queue_priority
 * 操作步骤     : 1、调用ffrt_task_attr_set_queue_priority接口设置队列优先级
 *               2、使用ffrt_task_attr_get_queue_priority查询优先级
 * 预期结果    : 查询结果与设定相同，值为3
 */
HWTEST_F(QueueTest, ffrt_task_attr_set_queue_priority, TestSize.Level0)
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
HWTEST_F(QueueTest, ffrt_queue_attr_set_max_concurrency, TestSize.Level0)
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
HWTEST_F(QueueTest, ffrt_queue_has_task, TestSize.Level0)
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
HWTEST_F(QueueTest, ffrt_queue_cancel_all_and_cancel_by_name, TestSize.Level0)
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
 * 测试用例名称 : ffrt_queue_cancel_all_and_cancel_by_name_concurrent
 * 测试用例描述 : 测试并发队列 ffrt_queue_cancel_all、ffrt_queue_cancel_by_name
 * 操作步骤     : 1、往队列中提交若干任务
 *               2、调用ffrt_queue_cancel_by_name取消指定任务
 *               3、调用ffrt_queue_cancel_all取消所有任务
 * 预期结果    : 任务取消成功
 */
HWTEST_F(QueueTest, ffrt_queue_cancel_all_and_cancel_by_name_concurrent, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    uint64_t concurrency = 1;
    ffrt_queue_attr_set_max_concurrency(&queue_attr, concurrency);

    ffrt_queue_t queue_handle = ffrt_queue_create(
        static_cast<ffrt_queue_type_t>(ffrt_queue_concurrent), "test_queue", &queue_attr);
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

    ffrt_queue_cancel_all(queue_handle);

    lock.unlock();
    ffrt_queue_cancel_and_wait(queue_handle);
    ffrt_queue_wait(handle);

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
HWTEST_F(QueueTest, ffrt_queue_submit_head, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(
        static_cast<ffrt_queue_type_t>(ffrt_queue_eventhandler_adapter), "test_queue", &queue_attr);

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

    ffrt_task_attr_t task_attr;
    ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_queue_priority(&task_attr, ffrt_queue_priority_immediate);
    ffrt_task_attr_set_name(&task_attr, "basic_function");
    ffrt_queue_submit_head(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);

    ffrt_queue_submit_head(queue_handle, create_function_wrapper(assignFuncs[0], ffrt_function_kind_queue), &task_attr);
    ffrt_queue_submit_head(queue_handle, create_function_wrapper(assignFuncs[1], ffrt_function_kind_queue), &task_attr);

    ffrt_task_attr_set_queue_priority(&task_attr, ffrt_queue_priority_high);
    ffrt_queue_submit_head(queue_handle, create_function_wrapper(assignFuncs[2], ffrt_function_kind_queue), &task_attr);
    ffrt_queue_submit_head(queue_handle, create_function_wrapper(assignFuncs[3], ffrt_function_kind_queue), &task_attr);

    ffrt_task_attr_set_queue_priority(&task_attr, ffrt_queue_priority_low);
    ffrt_queue_submit_head(queue_handle, create_function_wrapper(assignFuncs[4], ffrt_function_kind_queue), &task_attr);

    ffrt_task_attr_set_queue_priority(&task_attr, ffrt_queue_priority_immediate);
    ffrt_queue_submit_head(queue_handle, create_function_wrapper(assignFuncs[5], ffrt_function_kind_queue), &task_attr);

    ffrt_task_attr_set_queue_priority(&task_attr, ffrt_queue_priority_idle);
    ffrt_task_handle_t handle = ffrt_queue_submit_head_h(queue_handle,
        create_function_wrapper(assignFuncs[6], ffrt_function_kind_queue), &task_attr);
    ffrt_queue_submit_head(queue_handle, create_function_wrapper(assignFuncs[7], ffrt_function_kind_queue), &task_attr);

    lock.unlock();
    ffrt_queue_wait(handle);
    EXPECT_EQ(results, expectResults);

    ffrt_task_attr_destroy(&task_attr);
    ffrt_task_handle_destroy(handle);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

HWTEST_F(QueueTest, ffrt_get_main_queue, TestSize.Level0)
{
    // ffrt test case begin
    auto serialQueue = std::make_unique<ffrt::queue>("ffrt_normal_queue");
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
    usleep(100000);
}

HWTEST_F(QueueTest, get_main_queue, TestSize.Level1)
{
    auto serialQueue = std::make_unique<ffrt::queue>("ffrt_normal_queue");
    queue* mainQueue = ffrt::queue::get_main_queue();
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
            if (mainQueue != nullptr) {
                mainQueue->submit(basicFunc);
            }
        },
        ffrt::task_attr().qos(3).name("ffrt main_queue."));

    serialQueue->wait(handle);
    EXPECT_EQ(result, 1);
    usleep(100000);
}

HWTEST_F(QueueTest, ffrt_get_current_queue, TestSize.Level0)
{
    // ffrt test case begin
    auto serialQueue = std::make_unique<ffrt::queue>("ffrt_normal_queue");
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
}

/*
 * 测试用例名称 : ffrt_queue_recordtraffic_normal_trigger
 * 测试用例描述 : 设置串行队列的traffic_interval并生效
 * 操作步骤     : 1、创建队列
 *               2、提交堆积任务
 * 预期结果    : 成功触发流量监控告警
 */
HWTEST_F(QueueTest, ffrt_queue_recordtraffic_normal_trigger, TestSize.Level0)
{
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 30000000;
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
 *               2、调用ffrt_queue_attr_set_traffic_interval接口设置串行队列的流量监控窗口为0
 *               3、提交堆积任务
 * 预期结果    : 不触发流量监控告警
 */
HWTEST_F(QueueTest, ffrt_queue_recordtraffic_normal_corner, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    const int numTasks = 19;
    ffrt_task_handle_t handle[numTasks];
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
    for (int i = 0; i < numTasks; i++) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr);
    }
    usleep(1000000);
    ffrt_queue_submit(queue_handle, create_function_wrapper(firstFunc, ffrt_function_kind_queue), &task_attr);
    for (int i = 0; i < numTasks; i++) {
        handle[i] = ffrt_queue_submit_h(queue_handle, create_function_wrapper(fastFunc,
            ffrt_function_kind_queue), &task_attr);
    }
    ffrt_queue_wait(handle[numTasks - 1]);

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
 *               2、调用ffrt_queue_attr_set_traffic_interval接口设置串行队列的流量监控窗口
 *               3、提交堆积任务
 * 预期结果    : 成功触发流量监控告警，但不触发上报
 */
HWTEST_F(QueueTest, ffrt_queue_recordtraffic_delay_trigger, TestSize.Level0)
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
HWTEST_F(QueueTest, ffrt_queue_monitor_schedule_timeout111, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000000);
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 4);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;

    for (int i = 0; i < 9; i++) {
        ffrt::submit([]() {
            usleep(1100000);
        }, {}, {});
    }
    auto testQueue = std::make_unique<queue>("test_queue");

    auto t = testQueue->submit_h([&x] {
        FFRT_LOGE("task start"); x = x + 1;}, {});
    testQueue->wait(t);
    EXPECT_EQ(x, 1);
    ffrt::wait();
}

/*
 * 测试用例名称 : ffrt_queue_monitor_execute_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 成功触发任务执行超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_execute_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;
    auto testQueue = std::make_unique<queue>("test_queue");
    auto t = testQueue->submit_h([&x] { x = x + 1; usleep(2000000); FFRT_LOGE("done");}, {});
    FFRT_LOGE("submitted");
    testQueue->wait(t);
    FFRTFacade::GetQMInstance().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_delay_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 不触发超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_delay_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;
    auto testQueue = std::make_unique<queue>("test_queue");
    FFRT_LOGE("submit");
    auto t = testQueue->submit_h([&x] { FFRT_LOGE("delay end"); usleep(2100000);
        x = x + 1;}, task_attr().delay(1200000));
    testQueue->wait(t);
    FFRTFacade::GetQMInstance().timeoutUs_ = 30000000;
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称 : ffrt_queue_monitor_cancel_timeout
 * 测试用例描述 : 串行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务
 * 预期结果    : 不触发超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_cancel_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;
    auto testQueue = std::make_unique<queue>("test_queue");
    FFRT_LOGE("submit");
    auto t1 = testQueue->submit_h([&x] { x = x + 1; FFRT_LOGE("start"); });
    auto t2 = testQueue->submit_h([&x] { x = x + 1; FFRT_LOGE("delay start"); }, task_attr().delay(5000000));
    testQueue->cancel(t2);
    testQueue->wait(t1);
    testQueue->wait(t2);
    usleep(1200000);
    FFRTFacade::GetQMInstance().timeoutUs_ = 30000000;
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

void stall_us1(size_t us)
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
HWTEST_F(QueueTest, ffrt_queue_monitor_two_stage_timeout, TestSize.Level0)
{
    int x = 0;
    ffrt_task_timeout_set_cb(MyCallback);
    ffrt_task_timeout_set_threshold(1000);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;

    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 1);

    ffrt_queue_attr_t queue_attr;
    ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    ffrt::submit([] { stall_us1(1300 * 1000); });
    std::function<void()>&& basicFunc = [&x] { x = x + 1; stall_us1(1300 * 1000); FFRT_LOGE("done");};
    ffrt_task_handle_t task = ffrt_queue_submit_h(queue_handle,
        ffrt::create_function_wrapper(basicFunc, ffrt_function_kind_queue), nullptr);

    ffrt_queue_wait(task);
    ffrt_task_handle_destroy(task);
    ffrt_queue_destroy(queue_handle);
    ffrt_task_timeout_set_threshold(30000);
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
HWTEST_F(QueueTest, ffrt_spmc_queue_test, TestSize.Level0)
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
HWTEST_F(QueueTest, ffrt_spmc_queue_pop_head_to_another_queue, TestSize.Level0)
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

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue, 0, nullptr), 0);
    EXPECT_EQ(dstQueue.GetLength(), 0);

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue, 32, nullptr), 32);
    EXPECT_EQ(dstQueue.GetLength(), 32);

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue, 64, [] (void* data) { EXPECT_NE(data, nullptr); }), 32);
    EXPECT_EQ(dstQueue.GetLength(), 64);

    EXPECT_EQ(queue.PopHeadToAnotherQueue(dstQueue2, 128, nullptr), 64);
    EXPECT_EQ(dstQueue2.GetLength(), 64);
}

HWTEST_F(QueueTest, ffrt_queue_submit_h_f, TestSize.Level0)
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

HWTEST_F(QueueTest, ffrt_queue_submit_f, TestSize.Level0)
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

/*
 * 测试用例名称 : ffrt_queue_concurrent_recordtraffic_delay_trigger
 * 测试用例描述 : 设置并行队列的traffic_interval并生效
 * 操作步骤     : 1、创建队列
 *               2、调用ffrt_queue_attr_set_traffic_interval接口设置并行队列的流量监控窗口
 *               3、提交堆积任务
 * 预期结果    : 成功触发流量监控告警，但不触发上报
 */
HWTEST_F(QueueTest, ffrt_queue_concurrent_recordtraffic_delay_trigger, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    ffrt_task_handle_t handle;
    ffrt_task_attr_t task_attr;
    ffrt_task_attr_t task_attr_1;
    std::atomic<int> result = 0;
    uint64_t concurrency = 4;
    (void)ffrt_task_attr_init(&task_attr_1);
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_delay(&task_attr, 1200000);
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_attr_set_max_concurrency(&queue_attr, concurrency);

    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);
    ffrt::QueueHandler* queueHandler = reinterpret_cast<ffrt::QueueHandler*>(queue_handle);
    queueHandler->trafficRecordInterval_ = 1000000;
    queueHandler->trafficRecord_.nextUpdateTime_ = TimeStampCntvct() + 1000000;
    std::function<void()>&& testFunc = [&result]() {
        result = result + 1;
        usleep(1100000);
    };
    std::function<void()>&& fastFunc = [&result]() {
        result = result + 1;
    };

    ffrt_queue_submit(queue_handle, create_function_wrapper(testFunc, ffrt_function_kind_queue), &task_attr_1);
    for (int i = 0; i < 30; i++) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr);
    }
    usleep(1000000);
    handle = ffrt_queue_submit_h(queue_handle,
                create_function_wrapper(fastFunc, ffrt_function_kind_queue), &task_attr_1);
    ffrt_queue_wait(handle);
    ffrt_task_handle_destroy(handle);
    EXPECT_EQ(result, 2);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_schedule_timeout_all
 * 测试用例描述 : 并行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个任务占满全部worker，使得新并行任务等待调度
 * 预期结果    : 成功触发任务调度超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_concurrent_schedule_timeout_all, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 2);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 10> handles;

    for (uint32_t i = 0; i < 5; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                usleep(2000000);
                y.fetch_add(1);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 5; i < 10; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1000000);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 0; i < 10; i++) {
        testQueue->wait(handles[i]);
    }
    EXPECT_EQ(y, 10);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_schedule_timeout_part
 * 测试用例描述 : 并行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个任务占满部分worker，使得新并行任务等待调度
 * 预期结果    : 成功触发任务调度超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_concurrent_schedule_timeout_part, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));

    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 2);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 4> handles;

    for (uint32_t i = 0; i < 1; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(4000000);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 1; i < 4; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1000000);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 0; i < 4; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 4);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_schedule_timeout_delay
 * 测试用例描述 : 并行队列QueueMonitor检测到任务调度超时
 * 操作步骤     : 1、创建队列
 *               2、提交多个延迟任务，使得新并行任务等待调度
 * 预期结果    : 不触发任务调度超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_concurrent_schedule_timeout_delay, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 3);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 10> handles;

    for (uint32_t i = 0; i < 2; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Delayed_Task").delay(1100000)
        );
    }

    for (uint32_t i = 2; i < 10; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 0; i < 10; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 10);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_execute_timeout_all
 * 测试用例描述 : 并行队列QueueMonitor检测到任务执行超时
 * 操作步骤     : 1、创建队列
 *               2、提交执行时间长任务占满全部worker
 * 预期结果    : 成功触发任务执行超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_concurrent_execute_timeout_all, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 20> handles;

    for (uint32_t i = 0; i < 4; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 4; i < 15; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1100000);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 15; i < 20; i++) {
        ffrt::submit([&]() {
            y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }
    for (uint32_t i = 4; i < 15; i++) {
        testQueue->wait(handles[i]);
    }
    EXPECT_EQ(y, 20);
}
/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_cancel_timeout_all
 * 测试用例描述 : 并行队列QueueMonitor检测到任务超时
 * 操作步骤     : 1、创建队列
 *               2、先提交多个延迟任务占满全部worker，再执行前取消
 * 预期结果    : 不触发任务调度超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_concurrent_cancel_timeout_all, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    ffrt_set_cpu_worker_max_num(ffrt_qos_default, 2);
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 10> handles;

    for (uint32_t i = 0; i < 2; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Delayed_Task").delay(3000000)
        );
    }
    testQueue->cancel(handles[0]);
    testQueue->cancel(handles[1]);

    for (uint32_t i = 2; i < 10; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 2; i < 10; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 8);
}

/*
 * 测试用例名称 : ffrt_queue_concurrent_monitor_mixed_conditions_timeout
 * 测试用例描述 : 并行队列QueueMonitor检测到任务超时
 * 操作步骤     : 1、创建队列
 *               2、先提交多个延迟任务占满全部worker，再执行前取消
 * 预期结果    : 不触发任务超时告警
 */
HWTEST_F(QueueTest, ffrt_queue_monitor_concurrent_mixed_conditions_timeout, TestSize.Level1)
{
    ffrt_task_timeout_set_cb(MyCallback);
    FFRTFacade::GetDMInstance();
    FFRTFacade::GetQMInstance().timeoutUs_ = 1000000;

    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
    "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    std::atomic<std::uint64_t> y{0};
    std::array<ffrt::task_handle, 12> handles;

    for (uint32_t i = 0; i < 2; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
            }, ffrt::task_attr("Delayed_Task").delay(3000000)
        );
    }
    testQueue->cancel(handles[0]);

    for (uint32_t i = 2; i < 7; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(2000000);
            }, ffrt::task_attr("Slow_Task")
        );
    }

    for (uint32_t i = 7; i < 12; i++) {
        handles[i] = testQueue->submit_h(
            [&y] {
                y.fetch_add(1);
                usleep(1000000);
            }, ffrt::task_attr("Normal_Task")
        );
    }

    for (uint32_t i = 1; i < 12; i++) {
        testQueue->wait(handles[i]);
    }

    EXPECT_EQ(y, 11);

    FFRTFacade::GetQMInstance().timeoutUs_ = 30000000;
}

HWTEST_F(QueueTest, submit_task_while_concurrency_queue_waiting_all_test, TestSize.Level1)
{
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
        "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    bool notify = false;
    int waitingTaskCount = 0;
    std::atomic<int> submitThreadTaskCount = 0;
    std::mutex lock;
    std::condition_variable cv;
    for (int i = 0; i < 16; i++) {
        testQueue->submit([&] {
            std::unique_lock lk(lock);
            cv.wait(lk, [&] { return notify; });
            waitingTaskCount++;
            EXPECT_EQ(submitThreadTaskCount.load(), 0);
        });
    }

    std::mutex threadMutex;
    std::thread waitingThread([&] {
        std::unique_lock tm(threadMutex);
        EXPECT_EQ(ffrt_concurrent_queue_wait_all(*reinterpret_cast<ffrt_queue_t*>(testQueue.get())), 0);
    });

    std::thread submitThread([&] {
        while (threadMutex.try_lock()) {
            threadMutex.unlock();
            std::this_thread::yield();
        }
        usleep(100 * 1000);

        EXPECT_EQ(ffrt_concurrent_queue_wait_all(*reinterpret_cast<ffrt_queue_t*>(testQueue.get())), 1);
        for (int i = 0; i < 16; i++) {
            testQueue->submit([&] {
                submitThreadTaskCount.fetch_add(1);
            });
        }
    });

    submitThread.join();
    EXPECT_EQ(submitThreadTaskCount.load(), 0);

    {
        std::unique_lock lk(lock);
        notify = true;
        cv.notify_all();
    }
    waitingThread.join();
    EXPECT_EQ(waitingTaskCount, 16);

    EXPECT_EQ(ffrt_concurrent_queue_wait_all(*reinterpret_cast<ffrt_queue_t*>(testQueue.get())), 0);
    EXPECT_EQ(submitThreadTaskCount.load(), 16);
}
/*
 * 测试用例名称 : ffrt_queue_cancel_with_ffrt_skip_fail
 * 测试用例描述 : 使用ffrt::skip接口取消队列任务，返回失败
 * 操作步骤     : 1、创建队列
 *               2、提交延时的队列任务，同时调用skip接口取消队列任务
 * 预期结果    : 队列任务取消失败，任务成功执行
 */
HWTEST_F(QueueTest, ffrt_queue_cancel_with_ffrt_skip_fail, TestSize.Level0)
{
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
        "concurrent_queue", ffrt::queue_attr().max_concurrency(4));
    int result = 0;
    auto handle = testQueue->submit_h([&result] {
        result++;
    }, ffrt::task_attr("Delayed_Task").delay(1100000));

    EXPECT_EQ(ffrt::skip(handle), ffrt_error);
    testQueue->wait(handle);
    EXPECT_EQ(result, 1);
}

/*
 * 测试用例名称 : ffrt_queue_with_legacy_mode
 * 测试用例描述 : 队列设置legacymode为true，验证任务不在协程上执行
 * 操作步骤     : 1、创建队列， attr设置legacymode为true
 *               2、提交队列任务，验证当前是在线程上执行任务
 * 预期结果    : 任务执行成功，任务在线程上执行
 */
HWTEST_F(QueueTest, ffrt_queue_with_legacy_mode, TestSize.Level0)
{
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
        "concurrent_legacy_queue", ffrt::queue_attr().thread_mode(true));
    int result = 0;
    auto handle = testQueue->submit_h([&result] {
        ffrt::TaskBase* task = ffrt::ExecuteCtx::Cur()->task;
        EXPECT_EQ(static_cast<ffrt::QueueTask*>(task)->coRoutine, nullptr);
        EXPECT_EQ(static_cast<ffrt::QueueTask*>(task)->threadMode_, true);
        result++;
    }, ffrt::task_attr("Task_on_Thread"));
    testQueue->wait(handle);
    EXPECT_EQ(result, 1);
}

/*
 * 测试用例名称 : ffrt_queue_with_legacy_mode_off
 * 测试用例描述 : 队列设置legacymode为true，验证任务在协程上执行
 * 操作步骤     : 1、创建队列， attr设置legacymode为false
 *               2、提交队列任务，验证当前是在协程上执行任务
 * 预期结果    : 任务成功执行，任务在协程上执行
 */
HWTEST_F(QueueTest, ffrt_queue_with_legacy_mode_off, TestSize.Level0)
{
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_concurrent,
        "concurrent_normal_queue", ffrt::queue_attr());
    int result = 0;
    auto handle = testQueue->submit_h([&result] {
        ffrt::TaskBase* task = ffrt::ExecuteCtx::Cur()->task;
        if (USE_COROUTINE) {
            EXPECT_NE(static_cast<ffrt::QueueTask*>(task)->coRoutine, nullptr);
        } else {
            EXPECT_EQ(static_cast<ffrt::QueueTask*>(task)->coRoutine, nullptr);
        }
        EXPECT_EQ(static_cast<ffrt::QueueTask*>(task)->threadMode_, false);
        result++;
    }, ffrt::task_attr("Task_on_Coroutine"));
    testQueue->wait(handle);
    EXPECT_EQ(result, 1);
}

/*
 * 测试用例名称 : ffrt_queue_with_legacy_mode_mutex
 * 测试用例描述 : 队列设置legacymode为true，任务等锁并解锁后，判断任务之前状态是线程阻塞
 * 操作步骤     : 1、创建队列， attr设置legacymode为true
 *               2、提交延时的队列任务，验证当前是在线程上执行任务
 * 预期结果    : 任务成功执行，能够正确使用线程方式阻塞
 */
HWTEST_F(QueueTest, ffrt_queue_with_legacy_mode_mutex, TestSize.Level0)
{
    auto testQueue = std::make_unique<ffrt::queue>(ffrt::queue_serial,
        "serial_legacy_queue", ffrt::queue_attr().thread_mode(true));

    ffrt::mutex mtx;
    int result = 0;
    std::atomic<bool> flag = false;

    auto handle = testQueue->submit_h([&] {
        flag = true;
        while (flag) {
            usleep(100);
        }
        mtx.lock();
        ffrt::TaskBase* task = ffrt::ExecuteCtx::Cur()->task;
        EXPECT_EQ(task->preStatus, ffrt::TaskStatus::THREAD_BLOCK);
        EXPECT_EQ(static_cast<ffrt::QueueTask*>(task)->threadMode_, true);
        result++;
    }, ffrt::task_attr("Task_on_Thread"));

    while (!flag) {
        usleep(100);
    }

    {
        std::lock_guard lg(mtx);
        flag = false;
        usleep(10000);
    }
    testQueue->wait(handle);
    EXPECT_EQ(result, 1);
}

/*
 * 测试用例名称 : ffrt_eventhandler_adapter_queue_get_task_cnt
 * 测试用例描述 : eventhandler adapter队列获取任务数量
 * 预置条件     ：创建一个eventhandler adapter队列
 * 操作步骤     : 1、调用get_task_cnt接口
 *               2、提交若干延时任务后，再次调用get_task_cnt接口
 * 预期结果    : 1、初始get_task_cnt返回0，后续get_task_cnt返回提交的任务数
 *              2、提交延时任务后，get_task_cnt返回提交的任务数
 */
HWTEST_F(QueueTest, ffrt_eventhandler_adapter_queue_get_task_cnt, TestSize.Level0)
{
    std::shared_ptr<queue> testQueue = std::make_shared<queue>(
        static_cast<queue_type>(ffrt_inner_queue_type_t::ffrt_queue_eventhandler_adapter), "test_queue"
    );
    EXPECT_EQ(testQueue->get_task_cnt(), 0);

    for (int i = 0; i < 10; i++) {
        testQueue->submit([] { int x = 0; }, task_attr().delay(10 * 1000 * 1000));
        EXPECT_EQ(testQueue->get_task_cnt(), i + 1);
    }

    testQueue = nullptr;
}

/*
* 测试用例名称：queuetask_timeout_trigger_succ
* 测试用例描述：主动设置timeout的队列任务超时执行回调
* 预置条件    ：无
* 操作步骤    ：1、设置queuetimeout和tasktimeout
                2、提交任务超时触发回调
* 预期结果    ：超时回调成功被执行
*/
HWTEST_F(QueueTest, queuetask_timeout_trigger_succ, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_timeout(&attr, 1000000);
    ffrt_queue_attr_set_timeout(&queue_attr, 1000000);
    int result = 0;

    std::function<void()> timeoutCb = [&result]() { OnePlusForTest(static_cast<void*>(&result));
        printf("timeout callback\n"); };
    ffrt_queue_attr_set_callback(&queue_attr, ffrt::create_function_wrapper(timeoutCb, ffrt_function_kind_queue));
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    std::function<void()> basicFunc = []() { stall_us1(1100000); };

    ffrt_task_handle_t task1 =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &attr);
    ffrt_queue_wait(task1);
    ffrt_task_handle_destroy(task1);
    EXPECT_EQ(result, 1);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}