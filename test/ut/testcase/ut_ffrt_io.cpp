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
#include <cstdlib>
#include <mutex>
#include <cmath>
#include <chrono>
#include <thread>
#include <fstream>
#include <random>
#include <algorithm>
#include <cinttypes>
#include "util.h"
#include "ffrt_inner.h"
#include "c/ffrt_ipc.h"
#include "eu/co_routine.h"
#define private public
#include "eu/io_poller.h"
#include "sync/timer_manager.h"
#define protect public
#include "util/ffrt_facade.h"
#undef private
#undef protect
#include "../common.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
#ifdef APP_USE_ARM
#define SIZEOF_BYTES sizeof(uint32_t)
#else
#define SIZEOF_BYTES sizeof(uint64_t)
#endif

class ffrtIoTest : public testing::Test {
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

HWTEST_F(ffrtIoTest, IoPoller_1Producer_1Consumer, TestSize.Level0)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt::submit([&]() {
        ffrt::sync_io(testFd);
        uint64_t value = 0;
        ssize_t n = read(testFd, &value, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(value));
        EXPECT_EQ(value, expected);
        close(testFd);
        }, {}, {});
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(uint64_t));
        }, {}, {});

    ffrt::wait();
}

/*
* 测试用例名称：IoPoller_1Producer_1Consumer_Legacy_Mode
* 测试用例描述：sync_io接口，线程阻塞模式测试
* 预置条件    ：无
* 操作步骤    ：1.提交一个读任务，一个写任务
               2.读任务使设置legacy_mode后调用sync_io接口
* 预期结果    ：任务正确执行
*/
HWTEST_F(ffrtIoTest, IoPoller_1Producer_1Consumer_Legacy_Mode, TestSize.Level0)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt::submit([&]() {
        ffrt_this_task_set_legacy_mode(true);
        ffrt::sync_io(testFd);
        uint64_t value = 0;
        ssize_t n = read(testFd, &value, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(value));
        EXPECT_EQ(value, expected);
        close(testFd);
        }, {}, {});
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(uint64_t));
        }, {}, {});
    ffrt::wait();
}

HWTEST_F(ffrtIoTest, IoPoller_1Consumer_1Producer, TestSize.Level0)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(uint64_t));
        }, {}, {});

    stall_us(3);

    ffrt::submit([&]() {
        ffrt::sync_io(testFd);
        uint64_t value = 0;
        ssize_t n = read(testFd, &value, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(value));
        EXPECT_EQ(value, expected);
        close(testFd);
        }, {}, {});

    ffrt::wait();
}

uint64_t g_Ev = 0;

HWTEST_F(ffrtIoTest, IoPoller_Producer_N_Consumer_N, TestSize.Level0)
{
    int count = 3;
    uint64_t ev  = 0xabacadae;
    int testFd[count];
    uint64_t evN = 0;
    uint64_t i;
    for (i = 0; i < count; i++) {
        testFd[i] = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (testFd[i] < 0) {
            break;
        }
        evN++;
        g_Ev = evN;
        int fd = testFd[i];
        ffrt::submit([fd, i]() {
            ffrt::sync_io(fd);
            uint64_t value = 0;
            ssize_t n = read(fd, &value, sizeof(uint64_t));
            EXPECT_EQ(n, sizeof(value));
            close(fd);
            }, {}, {});
    }

    for (i = 0; i < evN; i++) {
        ffrt::submit([&, i]() {
            uint64_t expected = ev + i;
            ssize_t n = write(testFd[i], &expected, sizeof(uint64_t));
            EXPECT_EQ(n, sizeof(uint64_t));
            }, {}, {});
    }
    printf("max eventfd:%" PRIu64 ".\n", evN);
    ffrt::wait();
}

struct TestData {
    int fd;
    uint64_t expected;
    std::atomic<bool> finish;
};

static void testCallBack(void* token, uint32_t event)
{
    struct TestData* testData = reinterpret_cast<TestData*>(token);
    uint64_t value = 0;
    ssize_t n = read(testData->fd, &value, sizeof(uint64_t));
    EXPECT_EQ(n, sizeof(value));
    EXPECT_EQ(value, testData->expected);
    testData->finish = true;
    printf("cb done\n");
}

static void testfun(void* data)
{
    *(int*)data += 1;
    printf("%d, timeout callback\n", *(int*)data);
}
void (*cb)(void*) = testfun;

static void testSleepFun(void* data)
{
    usleep(100000);
    *(int*)data += 1;
    printf("%d, timeout callback\n", *(int*)data);
}
void (*sleepCb)(void*) = testSleepFun;

HWTEST_F(ffrtIoTest, ffrt_timer_start_succ_map_null, TestSize.Level0)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    uint64_t timeout = 20;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    EXPECT_EQ(0, ffrt_timer_start(qos, timeout, data, cb, false));

    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(30000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    while (1) {
        usleep(1000);
        if (testData.finish) break;
    }
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    EXPECT_EQ(1, x);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_start_fail_cb_null, TestSize.Level0)
{
    uint64_t timeout = 20;
    void* data = nullptr;
    void (*timeoutNullCb)(void*) = nullptr;

    EXPECT_EQ(-1, ffrt_timer_start(ffrt_qos_default, timeout, data, timeoutNullCb, false));
}

HWTEST_F(ffrtIoTest, ffrt_timer_start_fail_flag_teardown, TestSize.Level0)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    ffrt::FFRTFacade::GetTMInstance().teardown = true;
    uint64_t timeout = 20;
    void* data = nullptr;

    EXPECT_EQ(-1, ffrt_timer_start(qos, timeout, data, cb, false));
    ffrt::FFRTFacade::GetTMInstance().teardown = false;
}


HWTEST_F(ffrtIoTest, ffrt_timer_stop_fail, TestSize.Level0)
{
    int handle = -1;
    auto ret = ffrt_timer_stop(ffrt_qos_default, handle);
    EXPECT_EQ(ret, -1);
}

HWTEST_F(ffrtIoTest, ffrt_timer_query_test, TestSize.Level0)
{
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout1 = 10;
    uint64_t timeout2 = 20;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    ffrt_qos_t qos = ffrt_qos_default;
    int handle = ffrt_timer_start(qos, timeout1, data, cb, false);
    EXPECT_EQ(0, ffrt_timer_query(qos, handle));

    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    while (1) {
        usleep(1000);
        if (testData.finish) break;
    }
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    close(testFd);
    EXPECT_EQ(1, ffrt_timer_query(qos, handle));
}

HWTEST_F(ffrtIoTest, ffrt_timer_query_stop, TestSize.Level0)
{
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout1 = 10;
    uint64_t timeout2 = 20;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    ffrt_qos_t qos = ffrt_qos_default;
    int handle = ffrt_timer_start(qos, timeout1, data, sleepCb, false);
    EXPECT_EQ(0, ffrt_timer_query(qos, handle));
    usleep(500000);
    EXPECT_EQ(1, ffrt_timer_query(qos, handle));

    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    while (1) {
        usleep(1000);
        if (testData.finish) break;
    }
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    EXPECT_EQ(1, ffrt_timer_query(qos, handle));
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_poller_deregister_qos_valid, TestSize.Level0)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt_qos_t qos_level = ffrt_qos_default;
    int op = EPOLL_CTL_ADD;

    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};

    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    while (1) {
        usleep(1000);
        if (testData.finish) break;
    }
    ret = ffrt_epoll_ctl(qos_level, EPOLL_CTL_DEL, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_poller_deregister_qos_fd_invalid, TestSize.Level0)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt_qos_t qos_level = ffrt_qos_default;
    int op = EPOLL_CTL_ADD;

    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};

    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    while (1) {
        usleep(1000);
        if (testData.finish) break;
    }
    ret = ffrt_epoll_ctl(qos_level, EPOLL_CTL_DEL, -1, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(-1, ret);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_poller_deregister_qos_qos_invalid, TestSize.Level0)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt_qos_t qos_level = ffrt_qos_user_initiated;
    int op = EPOLL_CTL_ADD;

    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};

    int ret = ffrt_epoll_ctl(ffrt_qos_default, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    while (1) {
        usleep(1000);
        if (testData.finish) break;
    }
    ret = ffrt_epoll_ctl(qos_level, EPOLL_CTL_DEL, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_task_attr_set_local_true, TestSize.Level0)
{
    bool isLocalSet = true;
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_local(&attr, isLocalSet);
    bool localval = ffrt_task_attr_get_local(&attr);
    EXPECT_EQ(localval, isLocalSet);
}

HWTEST_F(ffrtIoTest, ffrt_task_attr_set_local_false, TestSize.Level0)
{
    bool isLocalSet = false;
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_local(&attr, isLocalSet);
    bool localval = ffrt_task_attr_get_local(&attr);
    EXPECT_EQ(localval, isLocalSet);
}

HWTEST_F(ffrtIoTest, ffrt_task_attr_set_local_attr_invalid, TestSize.Level0)
{
    bool isLocalSet = true;
    ffrt_task_attr_set_local(nullptr, isLocalSet);
    EXPECT_EQ(isLocalSet, true);
}

struct WakeData {
    int fd;
    void* data;
};

HWTEST_F(ffrtIoTest, ffrt_epoll_ctl_op1, TestSize.Level0)
{
    int op = EPOLL_CTL_ADD;
    ffrt_qos_t qos = ffrt_qos_default;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};

    int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_ctl_op3, TestSize.Level0)
{
    int op = EPOLL_CTL_MOD;
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};

    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_ctl_op_invalid, TestSize.Level0)
{
    int op = 0;
    ffrt_qos_t qos = ffrt_qos_default;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct TestData testData {.fd = testFd, .expected = expected, .finish = false};

    int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(-1, ret);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_get_count, TestSize.Level0)
{
    ffrt_qos_t qos = ffrt_qos_default;
    /* If this test is run in isolation, one can expect the poll count to be one.
     * However, if this test is run in the same environment of preceding tests e.g.,  ffrt_timer_start_succ_map_null the
     * count is expected to be larger than one. Considering both run modes, we expect a non-zero value, instead of a
     * specific value.
     * Note: In general, it is a better idea to make test run oblivious and function independently from
     * each-other.
     */
    ffrt::FFRTFacade::GetPPInstance().PollOnce(0);
    int ret = ffrt_epoll_get_count(qos);
    EXPECT_NE(ret, 0);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_get_wait_time_invalid, TestSize.Level0)
{
    void* taskHandle = nullptr;

    int ret = ffrt_epoll_get_wait_time(taskHandle);
    EXPECT_EQ(ret, 0);
}

/*
* 测试用例名称：ffrt_epoll_get_wait_time_valid
* 测试用例描述：使用有效任务指针调用ffrt_epoll_get_wait_time
* 预置条件    ：无
* 操作步骤    ：1.提交任务
               2、调用ffrt_epoll_get_wait_time接口
* 预期结果    ：结果为0
*/
HWTEST_F(ffrtIoTest, ffrt_epoll_get_wait_time_valid, TestSize.Level0)
{
    auto handle = ffrt::submit_h([]() {});

    int ret = ffrt_epoll_get_wait_time(handle);
    EXPECT_EQ(ret, 0);
    ffrt::wait();
}

typedef struct {
    int timerId;
    std::atomic<int> result;
} TimerDataT;

static void TimerCb(void* data)
{
    (reinterpret_cast<TimerDataT*>(data))->result++;
}

/*
* 测试用例名称：timer_repeat
* 测试用例描述：测试注册repeat的定时器后，进程能正确推出
* 预置条件    ：无
* 操作步骤    ：1.子进程提交若干个repeat为true的timer，sleep1秒后退出
               2.父进程等待子进程完成
* 预期结果    ：子进程结束码为0（不发生crash）
*/
HWTEST_F(ffrtIoTest, timer_repeat, TestSize.Level0)
{
    const int timerCount = 1000;
    static TimerDataT timerDatas[timerCount];
    for (auto& timerData : timerDatas) {
        timerData.result = 0;
        timerData.timerId = ffrt_timer_start(ffrt_qos_default,
            0, reinterpret_cast<void*>(&timerData), TimerCb, true);
    }
    while (timerDatas[0].result == 0) {
        usleep(1);
    }
    sleep(1);
    EXPECT_GT(timerDatas[0].result, 0);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_wait_test, TestSize.Level0)
{
    uint64_t expected = 0x3;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    int op = EPOLL_CTL_ADD;
    ffrt_qos_t qos = ffrt_qos_user_initiated;

    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt::submit([=]() {
        int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        // sleep to wait event
        ffrt_usleep(50000);
        // get fds
        struct epoll_event events[1024];
        int nfds = ffrt_epoll_wait(qos, events, 1024, -1);
        EXPECT_EQ(nfds, 1);
        ret = ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        // read vaule from fd and check
        uint64_t value = 0;
        ssize_t n = read(testFd, &value, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(value));
        EXPECT_EQ(value, expected);
        close(testFd);
    }, {}, {}, ffrt::task_attr().qos(ffrt_qos_user_initiated));

    stall_us(200);
    // write value to trigger poller mask
    uint8_t startCnt = ffrt_epoll_get_count(ffrt_qos_user_initiated);
    uint64_t u1 = 1;
    ssize_t n = write(testFd, &u1, sizeof(uint64_t));
    uint64_t u2 = 2;
    n = write(testFd, &u2, sizeof(uint64_t));
    EXPECT_EQ(n, sizeof(uint64_t));
    ffrt::wait();
    uint8_t endCnt = ffrt_epoll_get_count(ffrt_qos_user_initiated);
    auto wakeCnt = endCnt - startCnt;
    EXPECT_TRUE(wakeCnt < 10);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_wait_test_legacy_mode, TestSize.Level0)
{
    uint64_t expected = 0x3;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    int op = EPOLL_CTL_ADD;
    ffrt_qos_t qos = ffrt_qos_user_initiated;

    struct TestData testLegacyData {.fd = testFd, .expected = expected};
    ffrt::submit([=]() {
        ffrt_this_task_set_legacy_mode(true);
        int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        // sleep to wait event
        ffrt_usleep(50000);
        // get fds
        struct epoll_event events[1024];
        int nfds = ffrt_epoll_wait(qos, events, 1024, -1);
        EXPECT_EQ(nfds, 1);
        ret = ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
        EXPECT_EQ(ret, 0);
        // read vaule from fd and check
        uint64_t value = 0;
        ssize_t n = read(testFd, &value, sizeof(uint64_t));
        EXPECT_EQ(n, sizeof(value));
        EXPECT_EQ(value, expected);
        close(testFd);
        ffrt_this_task_set_legacy_mode(false);
    }, {}, {}, ffrt::task_attr().qos(ffrt_qos_user_initiated));

    stall_us(200);
    // write value to trigger poller mask
    uint8_t startCnt = ffrt_epoll_get_count(ffrt_qos_user_initiated);
    uint64_t u1 = 1;
    ssize_t n = write(testFd, &u1, sizeof(uint64_t));
    uint64_t u2 = 2;
    n = write(testFd, &u2, sizeof(uint64_t));
    EXPECT_EQ(n, sizeof(uint64_t));
    ffrt::wait();
    uint8_t endCnt = ffrt_epoll_get_count(ffrt_qos_user_initiated);
    auto wakeCnt = endCnt - startCnt;
    EXPECT_TRUE(wakeCnt < 10);
}
