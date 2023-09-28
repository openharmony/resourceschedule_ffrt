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

#include<thread>
#include<chrono>
#include<gtest/gtest.h>
#include"ffrt_inner.h"

using namespace std;
using namespace ffrt;
using namespace testing;

class CoroutineTest : public testing::Test {
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

    virtual void TeadDown()
    {
    }
};

const int BLOCKED_COUNT = 3;

typedef struct {
    int count;
}StacklessCoroutine1;

ffrt_coroutine_ret_t stackless_coroutine(void *co)
{
    ((StacklessCoroutine1*)(co))->count++;

    if (((StacklessCoroutine1*)(co))->count < BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    } else if (((StacklessCoroutine1*)(co))->count == BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    } else {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_ready;
    }
    return ffrt_coroutine_pending;
}

ffrt_coroutine_ret_t exec_stackless_coroutine(void *co)
{
    return stackless_coroutine(co);
}

void destroy_stackless_coroutine(void *co)
{
}

TEST_F(CoroutineTest, coroutine_submit_succ)
{
    StacklessCoroutine1 co1 = {0};
    StacklessCoroutine1 co2 = {0};
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_submit_coroutine((void *)co1, exec_stackless_coroutine, destroy_stackless_coroutine, NULL, NULL, &attr);
    ffrt_task_handle_t task1 = ffrt_submit_h_coroutine((void *)co2, exec_stackless_coroutine,
        destroy_stackless_coroutine, NULL, NULL, &attr);
    ffrt_poller_wakeup();
    usleep(100000);
    EXPECT_EQ(co1.count, 4);
    EXPECT_EQ(co2.count, 4);
    ffrt_task_handle_destroy(task1);
}

TEST_F(CoroutineTest, coroutine_submit_fail)
{
    EXPECT_SQ(ffrt_task_get(), nullptr);

    StacklessCoroutine1 co1 = {0};
    StacklessCoroutine1 co2 = {0};
    StacklessCoroutine1 co3 = {0};
    StacklessCoroutine1 co4 = {0};
    StacklessCoroutine1 co5 = {0};
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);

    ffrt_submit_coroutine(nullptr, nullptr, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_t task1 = ffrt_submit_h_coroutine(nullptr, nullptr, nullptr, NULL, NULL, &attr);

    ffrt_submit_coroutine((void *)&co1, nullptr, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_t task2 = ffrt_submit_h_coroutine((void *)&co2, nullptr, nullptr, NULL, NULL, &attr);

    ffrt_submit_coroutine((void *)&co3, exec_stackless_coroutine, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_t task3 = ffrt_submit_h_coroutine((void *)&co4, exec_stackless_coroutine,
        nullptr, NULL, NULL, &attr);
}

StacklessCoroutine1 g_col = {0};
struct Waker {
    void *phandle;
    void *handle;
} waker;

void wake_stackless_coroutine(void *arg)
{
    ffrt_wake_coroutine(((Waker *)arg)->phandle);
}

void destroy_wake_of_stackless_coroutine(void *arg)
{
    ffrt_task_handle_destroy(((Waker *)arg)->handle);
}

ffrt_coroutine_ret_t maintask_stackless_coroutine(void *co)
{
    ((StacklessCoroutine1*)(co))->count++;

    if (((StacklessCoroutine1*)(co))->count < BLOCKED_COUNT) {
        if (((StacklessCoroutine1*)(co))->count == 1) {
            ffrt_task_attr_t attr;
            ffrt_task_attr_init(&attr);
            ffrt_set_wake_flag(true);
            ffrt_task_handle_t h = ffrt_submit_h_coroutine((void *)&g_col, exec_stackless_coroutine,
                destroy_stackless_coroutine, NULL, NULL, &attr);
            waker.phandle = ffrt_task_get();
            waker.handle = h;
            ffrt_wake_by_handle(&waker, wake_stackless_coroutine, destroy_wake_of_stackless_coroutine, h);
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_pending;
        }
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    } else if (((StacklessCoroutine1*)(co))->count == BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    } else {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_ready;
    }
    return ffrt_coroutine_pending;
}

ffrt_coroutine_ret_t maintask_exec_stackless_coroutine(void *co)
{
    return maintask_stackless_coroutine(co);
}

void maintask_destroy_stackless_coroutine(void *co)
{
}

TEST_F(CoroutineTest, coroutine_wake_by_handle_succ)
{
    StacklessCoroutine1 co1 = {0};
    ffrt_task_attr_t maintask_attr;
    ffrt_task_attr_init(&maintask_attrr);
    ffrt_task_handle_t maintask = ffrt_submit_h_coroutine((void *)&co1, maintask_exec_stackless_coroutine,
        maintask_destroy_stackless_coroutine, NULL, NULL, &maintask_attr);
    ffrt_wait();
    usleep(100000);
    ffrt_task_handle_destroy(maintask);
}

ffrt_coroutine_ret_t maintask_stackless_coroutine_fail(void *co)
{
    ((StacklessCoroutine1*)(co))->count++;

    if (((StacklessCoroutine1*)(co))->count < BLOCKED_COUNT) {
            if (((StacklessCoroutine1*)(co))->count == 1) {
                ffrt_task_attr_t attr;
                ffrt_task_attr_init(&attr);
                ffrt_task_handle_t h = ffrt_submit_h_coroutine((void *)&g_col, exec_stackless_coroutine,
                    destroy_stackless_coroutine, NULL, NULL, &attr);
                waker.phandle = ffrt_task_get();
                waker.handle = h;

                ffrt_wake_by_handle(nullptr, nullptr, nullptr, nullptr);
                ffrt_wake_by_handle(&waker, nullptr, nullptr, h);
                ffrt_wake_by_handle(nullptr, nullptr, nullptr, nullptr);
                ffrt_wake_by_handle(&waker, nullptr, nullptr, h);
                ffrt_wake_by_handle(&waker, wake_stackless_coroutine, nullptr, h);
                ffrt_wake_by_handle(&waker, wake_stackless_coroutine, destroy_wake_of_stackless_coroutine, h);
                ffrt_wake_coroutine(ffrt_task_get());
                return ffrt_coroutine_pending;
            }
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_pending;
        } else if (((StacklessCoroutine1*)(co))->count == BLOCKED_COUNT) {
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_pending;
        } else {
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_ready;
        }
        return ffrt_coroutine_pending;
}

ffrt_coroutine_ret_t maintask_exec_stackless_coroutine_fail(void *co)
{
    return maintask_stackless_coroutine_fail(co);
}

void maintask_destroy_stackless_coroutine_fail(void *co)
{
}

TEST_F(CoroutineTest, coroutine_wake_by_handle_fail)
{
    StacklessCoroutine1 co1 = {0};
    ffrt_task_attr_t maintask_attr;
    ffrt_task_attr_init(&maintask_attr);
    ffrt_task_handle_t maintask = ffrt_submit_h_coroutine((void *)&co1, maintask_exec_stackless_coroutine_fail,
        maintask_destroy_stackless_coroutine_fail, NULL, NULL, &maintask_attr);
    ffrt_task_handle_destroy(maintask);
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

TEST_F(CoroutineTest, ffrt_poller_register_deregister)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
    }, {}, {})

    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_poller_register(testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    usleep(100);
    ffrt_poller_deregister(testFd);
    close(testFd);
}

TEST_F(CoroutineTest, timerFunc)
{
    auto time1 = reinterpret_cast<int(*)()(100000)>;
    int ret = ffrt_poller_register_timerfunc(time1);
    EXPECT_EQ(ret, 1);

    auto time2 = reinterpret_cast<int(*)()(100000)>;
    int ret = ffrt_poller_register_timerfunc(time2);
    EXPECT_EQ(ret, 0);
}