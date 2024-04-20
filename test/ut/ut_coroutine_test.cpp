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
    std::mutex lock;
}StacklessCoroutine1;

ffrt_coroutine_ret_t stackless_coroutine(void* co)
{
    StacklessCoroutine1* stacklesscoroutine = reinterpret_cast<StacklessCoroutine1*>(co);
    std::lock_guard lg(stacklesscoroutine->lock);
    printf("stacklesscoroutine %d\n", stacklesscoroutine->count);
    stacklesscoroutine->count++;

    if (stacklesscoroutine->count < BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_get_current_task());
        return ffrt_coroutine_pending;
    } else if (stacklesscoroutine->count == BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_get_current_task());
        return ffrt_coroutine_pending;
    } else {
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
    ffrt_submit_coroutine((void *)co2, exec_stackless_coroutine, destroy_stackless_coroutine, NULL, NULL, &attr);
    ffrt_poller_wakeup(ffrt_qos_default);
    usleep(100000);
    EXPECT_EQ(co1.count, 4);
    EXPECT_EQ(co2.count, 4);
}

TEST_F(CoroutineTest, coroutine_submit_fail)
{
    EXPECT_SQ(ffrt_get_current_task(), nullptr);

    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);

    ffrt_submit_coroutine(nullptr, nullptr, nullptr, NULL, NULL, &attr);
    ffrt_submit_coroutine(nullptr, nullptr, nullptr, NULL, NULL, &attr);
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

TEST_F(CoroutineTest, ffrt_epoll_ctl_add_del)
{
    uint64_t expected = 0xabacadae;
    int testFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

    ffrt::submit([&]() {
        ssize_t n = write(testFd, &expected, sizeof(uint64_t));
        EXPECT_EQ(sizeof(n), sizeof(uint64_t));
    }, {}, {})

    struct TestData testData {.fd = testFd, .expected = expected};
    ffrt_epoll_ctl(ffrt_qos_default, EPOLL_CTL_ADD, testFd, EPOLLIN, reinterpret_cast<void*>(&testData), testCallBack);
    usleep(100);
    ffrt_epoll_ctl(ffrt_qos_default, EPOLL_CTL_DEL, testFd, 0, nullptr, nullptr);
    close(testFd);
}