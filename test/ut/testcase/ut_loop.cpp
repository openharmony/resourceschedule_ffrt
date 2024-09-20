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
#include <mockcpp/mockcpp.hpp>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include "ffrt_inner.h"
#include "c/loop.h"
#include "util/event_handler_adapter.h"
#include "../common.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

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
HWTEST_F(LoopTest, loop_null_queue_create_fail, TestSize.Level1)
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
HWTEST_F(LoopTest, loop_serial_queue_create_succ, TestSize.Level1)
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
HWTEST_F(LoopTest, loop_concurrent_queue_create_succ, TestSize.Level1)
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
 *              2、创建loop前向队列提交任务
 * 操作步骤    ：1、创建loop
 *
 * 预期结果    ：创建失败
 */
HWTEST_F(LoopTest, loop_concurrent_queue_create_fail, TestSize.Level1)
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
HWTEST_F(LoopTest, loop_run_fail, TestSize.Level1)
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
HWTEST_F(LoopTest, loop_destroy_fail, TestSize.Level1)
{
    int ret = ffrt_loop_destroy(nullptr);
    EXPECT_NE(ret, 0);
}

/*
 * 测试用例名称：loop_run_destroy_success
 * 测试用例描述：正常loop run成功、destroy
 * 预置条件    ：1、调用串行队列创建接口创建concurrent队列
 *              2、用队列创建loop
 * 操作步骤    ：1、启动线程执行loop run
 *              2、销毁loop成功
 * 预期结果    ：执行成功
 */
HWTEST_F(LoopTest, loop_run_destroy_success, TestSize.Level1)
{
    const uint64_t SLEEP_TIME = 250000;
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);

    auto loop = ffrt_loop_create(queue_handle);
    EXPECT_NE(loop, nullptr);

    pthread_t thread;
    pthread_create(&thread, 0, ThreadFunc, loop);

    ffrt_loop_stop(loop);
    usleep(SLEEP_TIME);
    int ret = ffrt_loop_destroy(loop);
    EXPECT_EQ(ret, 0);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}
 
 struct TestData {
    int fd;
    uint64_t expected;
};
 
static void testCallBack(void* token, uint32_t event)
{
    struct TestData* testData = reinterpret_cast<TestData*>(token);
    uint64_t value = 0;
    ssize_t n = read(testData->fd, &value, sizeof(uint64_t));
    EXPECT_EQ(n, sizeof(value));
    EXPECT_EQ(value, testData->expected);
}
 
int AddFdListener(void* handler, uint32_t fd, uint32_t event, void* data, ffrt_poller_cb cb)
{
    return 0;
}
 
int RemoveFdListener(void* handler, uint32_t fd)
{
    return 0;
}
 
HWTEST_F(LoopTest, ffrt_add_and_remove_fd, TestSize.Level1)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
    ffrt_queue_t queue_handle = ffrt_queue_create(
        static_cast<ffrt_queue_type_t>(ffrt_queue_eventhandler_interactive), "test_queue", &queue_attr);
    MOCKER(ffrt_get_main_queue).stubs().will(returnValue(queue_handle));
 
    EventHandlerAdapter::Instance()->AddFdListener = AddFdListener;
    EventHandlerAdapter::Instance()->RemoveFdListener = RemoveFdListener;
 
    ffrt_queue_t mainQueue = ffrt_get_main_queue();
    auto loop = ffrt_loop_create(mainQueue);
    EXPECT_TRUE(loop != nullptr);
    ffrt_loop_run(loop);
    int ret = 0;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    uint64_t expected = 0xabacadae;
    struct TestData testData {.fd = testFd, .expected = expected};
    ret = ffrt_loop_epoll_ctl(loop, EPOLL_CTL_ADD, testFd, EPOLLIN, (void*)(&testData), testCallBack);
    EXPECT_EQ(ret, 0);
    ssize_t n = write(testFd, &expected, sizeof(uint64_t));
    EXPECT_EQ(n, sizeof(uint64_t));
    usleep(25000);
 
    ret = ffrt_loop_epoll_ctl(loop, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    EXPECT_EQ(ret, 0);
 
    ffrt_loop_stop(loop);
    ffrt_loop_destroy(loop);
 
    GlobalMockObject::reset();
    GlobalMockObject::verify();
}
