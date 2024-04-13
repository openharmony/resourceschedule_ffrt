/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, sofware
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
#include "util.h"
#include "ffrt_inner.h"
#include "eu/co_routine.h"
#include "sync/io_poller.h"
#define private public
#define protect public
#include "sync/poller.h"
#undef private
#undef protect

using namespace std;
using namespace testing;

class ffrtIoTest : public testing::Test {
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
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    ffrt::PollerProxy::Instance()->GetPoller(qos).timerHandle_ = -1;
    ffrt::PollerProxy::Instance()->GetPoller(qos).timerMap_.clear();
    ffrt::PollerProxy::Instance()->GetPoller(qos).executedHandle_.clear();
    }
};

TEST_F(ffrtIoTest, IoPoller_1Producer_1Consumer)
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

TEST_F(ffrtIoTest, IoPoller_1Consumer_1Producer)
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

TEST_F(ffrtIoTest, IoPoller_Producer_N_Consumer_N)
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
    printf("max eventfd:%lu.\n", evN);
    ffrt::wait();
}

#ifdef FFRT_IO_TASK_SCHEDULER

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

TEST_F(ffrtIoTest, ffrt_timer_start_succ_map_null)
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
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(ffrt_qos_default, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(1, x);
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_start_fail_cb_null)
{
    uint64_t timeout = 20;
    void* data = nullptr;
    void (*timeoutNullCb)(void*) = nullptr;

    EXPECT_EQ(-1, ffrt_timer_start(ffrt_qos_default, timeout, data, timeoutNullCb, false));
}

TEST_F(ffrtIoTest, ffrt_timer_start_fail_flag_teardown)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::TEARDOWN;
    uint64_t timeout = 20;
    void* data = nullptr;

    EXPECT_EQ(-1, ffrt_timer_start(qos, timeout, data, cb, false));
}

TEST_F(ffrtIoTest, ffrt_timer_start_succ_short_timeout_flagwait)
{
    int x = 0;
    int* xf = &x;
    void* data = xf;
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    uint64_t timeout1 = 200;
    uint64_t timeout2 = 10;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::WAIT;
    EXPECT_EQ(0, ffrt_timer_start(qos, timeout1, data, cb, false));

    EXPECT_EQ(1, ffrt_timer_start(qos, timeout2, data, cb, false));
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(1, x);
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_start_succ_short_timeout_flagwake)
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
    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::WAKE;
    EXPECT_EQ(1, ffrt_timer_start(qos, timeout2, data, cb, false));
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(1, x);
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_start_succ_long_timeout_flagwake)
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
    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::WAKE;
    EXPECT_EQ(1, ffrt_timer_start(qos, timeout2, data, cb, false));
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(1, x);
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_stop_fail)
{
    int handle = -1;
    ffrt_timer_stop(ffrt_qos_default, handle);
}

TEST_F(ffrtIoTest, ffrt_timer_stop_succ_mapfirst_flagwait)
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
    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::WAIT;
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_stop_succ_mapother)
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
    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::WAIT;
    int handle = ffrt_timer_start(qos, timeout2, data, cb, false);
    EXPECT_EQ(1, handle);
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);
    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_stop_succ_mapfirst_flagwake)
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
    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::WAKE;
    int handle = ffrt_timer_start(qos, timeout2, data, cb, false);
    EXPECT_EQ(1, handle);
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_stop_succ_flag_teardown)
{
    ffrt::QoS qos = ffrt::ExecuteCtx::Cur()->qos;
    int x = 0;
    int* xf = &x;
    void* data = xf;
    uint64_t timeout2 = 20;
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    ffrt::PollerProxy::Instance()->GetPoller(qos).flag_ = ffrt::EpollStatus::TEARDOWN;
    int handle = ffrt_timer_start(qos, timeout2, data, cb, false);
    EXPECT_EQ(-1, handle);
    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(21000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(0, x);
    close(testFd);
}

TEST_F(ffrtIoTest, ffrt_timer_query_test)
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
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    close(testFd);
    EXPECT_EQ(1, ffrt_timer_query(qos, handle));
}

TEST_F(ffrtIoTest, ffrt_timer_query_stop)
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

    ffrt_timer_stop(qos, handle);
    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(qos, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);

    usleep(15000);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
        }, {}, {});
    ffrt_epoll_ctl(qos, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    ffrt::wait();
    EXPECT_EQ(-1, ffrt_timer_query(qos, handle));
    close(testFd);
}
#endif