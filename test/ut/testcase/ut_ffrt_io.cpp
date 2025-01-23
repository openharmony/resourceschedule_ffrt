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
#include "sync/io_poller.h"
#define private public
#define protect public
#include "util/ffrt_facade.h"
#undef private
#undef protect
#include "../common.h"

using namespace std;
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
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::WAKE;
    }

    void TearDown() override
    {
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).timerHandle_ = -1;
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).timerMutex_.lock();
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).timerMap_.clear();
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).timerMutex_.unlock();
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).executedHandle_.clear();
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::TEARDOWN;
    }
};

HWTEST_F(ffrtIoTest, IoPoller_1Producer_1Consumer, TestSize.Level1)
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

HWTEST_F(ffrtIoTest, IoPoller_1Consumer_1Producer, TestSize.Level1)
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

HWTEST_F(ffrtIoTest, IoPoller_Producer_N_Consumer_N, TestSize.Level1)
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
};

static void testCallBack(void* token, uint32_t event)
{
    struct TestData* testData = reinterpret_cast<TestData*>(token);
    uint64_t value = 0;
    ssize_t n = read(testData->fd, &value, sizeof(uint64_t));
    EXPECT_EQ(n, sizeof(value));
    EXPECT_EQ(value, testData->expected);
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

HWTEST_F(ffrtIoTest, ffrt_timer_start_succ_map_null, TestSize.Level1)
{
    uint64_t timeout = 20;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    EXPECT_EQ(0, ffrt_timer_start(ffrt_qos_default, timeout, data, cb, false));

    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(ffrt_qos_default, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(30000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(ffrt_qos_default, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(1, x);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_start_fail_cb_null, TestSize.Level1)
{
    uint64_t timeout = 20;
    void* data = nullptr;
    void (*timeoutNullCb)(void*) = nullptr;

    EXPECT_EQ(-1, ffrt_timer_start(ffrt_qos_default, timeout, data, timeoutNullCb, false));
}

HWTEST_F(ffrtIoTest, ffrt_timer_start_fail_flag_teardown, TestSize.Level1)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::TEARDOWN;
    uint64_t timeout = 20;
    void* data = nullptr;

    EXPECT_EQ(-1, ffrt_timer_start(qos, timeout, data, cb, false));
}

HWTEST_F(ffrtIoTest, ffrt_timer_start_succ_short_timeout_flagwait, TestSize.Level1)
{
    int x = 0;
    int* xf = &x;
    void* data = xf;
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    uint64_t timeout1 = 200;
    uint64_t timeout2 = 10;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::WAIT;
    EXPECT_EQ(0, ffrt_timer_start(qos, timeout1, data, cb, false));

    EXPECT_EQ(1, ffrt_timer_start(qos, timeout2, data, cb, false));
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(1, x);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_start_succ_short_timeout_flagwake, TestSize.Level1)
{
    int x = 0;
    int* xf = &x;
    void* data = xf;
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    uint64_t timeout1 = 400;
    uint64_t timeout2 = 10;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    EXPECT_EQ(0, ffrt_timer_start(qos, timeout1, data, cb, false));
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::WAKE;
    EXPECT_EQ(1, ffrt_timer_start(qos, timeout2, data, cb, false));
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(static_cast<int>(1), x);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_start_succ_long_timeout_flagwake, TestSize.Level1)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout1 = 10;
    uint64_t timeout2 = 200;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    EXPECT_EQ(0, ffrt_timer_start(qos, timeout1, data, cb, false));
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::WAKE;
    EXPECT_EQ(1, ffrt_timer_start(qos, timeout2, data, cb, false));
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(1, x);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_stop_fail, TestSize.Level1)
{
    int handle = -1;
    ffrt_timer_stop(ffrt_qos_default, handle);
}

HWTEST_F(ffrtIoTest, ffrt_timer_stop_succ_mapfirst_flagwait, TestSize.Level1)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout1 = 20;
    uint64_t timeout2 = 10;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    EXPECT_EQ(0, ffrt_timer_start(qos, timeout1, data, cb, false));

    int handle = ffrt_timer_start(qos, timeout2, data, cb, false);
    EXPECT_EQ(1, handle);
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::WAIT;
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_stop_succ_mapother, TestSize.Level1)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout1 = 10;
    uint64_t timeout2 = 20;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    EXPECT_EQ(0, ffrt_timer_start(qos, timeout1, data, cb, false));
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::WAIT;
    int handle = ffrt_timer_start(qos, timeout2, data, cb, false);
    EXPECT_EQ(1, handle);
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_stop_succ_mapfirst_flagwake, TestSize.Level1)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout1 = 10;
    uint64_t timeout2 = 20;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    EXPECT_EQ(0, ffrt_timer_start(qos, timeout1, data, cb, false));
    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::WAKE;
    int handle = ffrt_timer_start(qos, timeout2, data, cb, false);
    EXPECT_EQ(1, handle);
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_stop_succ_flag_teardown, TestSize.Level1)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout2 = 20;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    ffrt::FFRTFacade::GetPPInstance().GetPoller(qos).flag_ = ffrt::EpollStatus::TEARDOWN;
    int handle = ffrt_timer_start(qos, timeout2, data, cb, false);
    EXPECT_EQ(-1, handle);
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(21000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(0, x);
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_timer_query_test, TestSize.Level1)
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

    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
    EXPECT_EQ(1, ffrt_timer_query(qos, handle));
}

HWTEST_F(ffrtIoTest, ffrt_timer_query_stop, TestSize.Level1)
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
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(-1, ffrt_timer_query(qos, handle));
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_poller_deregister_qos_valid, TestSize.Level1)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt_qos_t qos_level = ffrt_qos_default;
    int op = EPOLL_CTL_ADD;

    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});

    usleep(100);

    ret = ffrt_epoll_ctl(qos_level, EPOLL_CTL_DEL, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);
    ffrt::wait();
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_poller_deregister_qos_fd_invalid, TestSize.Level1)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt_qos_t qos_level = ffrt_qos_default;
    int op = EPOLL_CTL_ADD;

    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});

    usleep(100);

    ret = ffrt_epoll_ctl(qos_level, EPOLL_CTL_DEL, -1, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(-1, ret);
    ffrt::wait();
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_poller_deregister_qos_qos_invalid, TestSize.Level1)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt_qos_t qos_level = ffrt_qos_user_initiated;
    int op = EPOLL_CTL_ADD;

    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(ffrt_qos_default, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        }, {}, {});

    usleep(100);

    ret = ffrt_epoll_ctl(qos_level, EPOLL_CTL_DEL, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(-1, ret);
    ffrt::wait();
    close(testFd);
}

HWTEST_F(ffrtIoTest, ffrt_task_attr_set_local_true, TestSize.Level1)
{
    bool isLocalSet = true;
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_local(&attr, isLocalSet);
    bool localval = ffrt_task_attr_get_local(&attr);
    EXPECT_EQ(localval, isLocalSet);
}

HWTEST_F(ffrtIoTest, ffrt_task_attr_set_local_false, TestSize.Level1)
{
    bool isLocalSet = false;
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_local(&attr, isLocalSet);
    bool localval = ffrt_task_attr_get_local(&attr);
    EXPECT_EQ(localval, isLocalSet);
}

HWTEST_F(ffrtIoTest, ffrt_task_attr_set_local_attr_invalid, TestSize.Level1)
{
    bool isLocalSet = true;
    ffrt_task_attr_set_local(nullptr, isLocalSet);
    EXPECT_EQ(isLocalSet, true);
}

struct WakeData {
    int fd;
    void* data;
};

HWTEST_F(ffrtIoTest, ffrt_epoll_wait_valid, TestSize.Level1)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct WakeData m_wakeData;
    m_wakeData.data = nullptr;
    m_wakeData.fd = testFd;
    ffrt_qos_t qos_level = ffrt_qos_user_initiated;
    int op = EPOLL_CTL_ADD;
    epoll_event ev = { .events = EPOLLIN, .data = {.ptr = static_cast<void*>(&m_wakeData)} };
    int maxevents = 1024;
    uint64_t timeout = 0;
    int result = 0;
    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        result = ffrt_epoll_wait(qos_level, &ev, maxevents, timeout);
        }, {}, {});
    usleep(1000);
    EXPECT_EQ(0, result);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_wait_events_invalid, TestSize.Level1)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct WakeData m_wakeData;
    m_wakeData.data = nullptr;
    m_wakeData.fd = testFd;
    ffrt_qos_t qos_level = ffrt_qos_user_initiated;
    int op = EPOLL_CTL_ADD;
    epoll_event ev = { .events = EPOLLIN, .data = {.ptr = static_cast<void*>(&m_wakeData)} };
    int maxevents = 1024;
    uint64_t timeout = 0;
    int result = 0;

    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        result = ffrt_epoll_wait(qos_level, nullptr, maxevents, timeout);
        }, {}, {});
    usleep(1000);
    EXPECT_EQ(-1, result);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_wait_maxevents_invalid, TestSize.Level1)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    struct WakeData m_wakeData;
    m_wakeData.data = nullptr;
    m_wakeData.fd = testFd;
    ffrt_qos_t qos_level = ffrt_qos_user_initiated;
    int op = EPOLL_CTL_ADD;
    int result = 0;

    epoll_event ev = { .events = EPOLLIN, .data = {.ptr = static_cast<void*>(&m_wakeData)} };
    int maxevents = -1;
    uint64_t timeout = 0;
    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        result = ffrt_epoll_wait(qos_level, &ev, maxevents, timeout);
        }, {}, {});
    usleep(1000);
    EXPECT_EQ(-1, result);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_ctl_op1, TestSize.Level1)
{
    int op = EPOLL_CTL_ADD;
    ffrt_qos_t qos = ffrt_qos_default;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_ctl_op3, TestSize.Level1)
{
    int op = EPOLL_CTL_MOD;
    ffrt_qos_t qos = ffrt_qos_default;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct TestData testData {.fd = testFd, .expected = expected};

    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_ctl_op_invalid, TestSize.Level1)
{
    int op = 0;
    ffrt_qos_t qos = ffrt_qos_default;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct TestData testData {.fd = testFd, .expected = expected};

    int ret = ffrt_epoll_ctl(qos, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(-1, ret);
}

/**
 * @tc.name: ffrt_epoll_wait_valid_with_thread_mode
 * @tc.desc: Test ffrt_epoll_wait when valid in Thread mode
 * @tc.type: FUNC
 */
HWTEST_F(ffrtIoTest, ffrt_epoll_wait_valid_with_thread_mode, TestSize.Level1)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    struct WakeData m_wakeData;
    m_wakeData.data = nullptr;
    m_wakeData.fd = testFd;
    ffrt_qos_t qos_level = ffrt_qos_user_initiated;
    int op = EPOLL_CTL_ADD;
    epoll_event ev = { .events = EPOLLIN, .data = {.ptr = static_cast<void*>(&m_wakeData)} };
    int maxevents = 1024;
    uint64_t timeout = 0;
    int result = 0;
    struct TestData testData {.fd = testFd, .expected = expected};


    int ret = ffrt_epoll_ctl(qos_level, op, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    EXPECT_EQ(0, ret);

    ffrt::submit([&]() {
        ffrt_this_task_set_legacy_mode(true);
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), SIZEOF_BYTES);
        result = ffrt_epoll_wait(qos_level, &ev, maxevents, timeout);
        ffrt_this_task_set_legacy_mode(false);
        }, {}, {});
    usleep(1000);
    EXPECT_EQ(0, result);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_get_count, TestSize.Level1)
{
    ffrt_qos_t qos = ffrt_qos_default;

    int  ret = ffrt_epoll_get_count(qos);
    EXPECT_NE(ret, 0);
}

HWTEST_F(ffrtIoTest, ffrt_epoll_get_wait_time_invalid, TestSize.Level1)
{
    void* taskHandle = nullptr;

    int ret = ffrt_epoll_get_wait_time(taskHandle);
    EXPECT_EQ(ret, 0);
}