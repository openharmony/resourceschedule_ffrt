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
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include "c/executor_task.h"
#include "ffrt_inner.h"
#include "eu/cpu_worker.h"
#include "eu/co_routine.h"
#include "sched/scheduler.h"
#include "../common.h"
#include "tm/scpu_task.h"

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
class CoroutineTest : public testing::Test {
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
constexpr int BLOCKED_COUNT = 3;

typedef struct {
    std::atomic<int> count;
    std::mutex lock;
} StacklessCoroutine1;

ffrt_coroutine_ret_t stackless_coroutine(void* co)
{
    StacklessCoroutine1* stacklesscoroutine = reinterpret_cast<StacklessCoroutine1*>(co);
    std::lock_guard lg(stacklesscoroutine->lock);
    printf("stacklesscoroutine %d\n", stacklesscoroutine->count.load());
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

HWTEST_F(CoroutineTest, coroutine_submit_succ, TestSize.Level0)
{
    // coroutine_submit_004
    StacklessCoroutine1 co1 = {0};
    StacklessCoroutine1 co2 = {0};
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_submit_coroutine((void *)&co1, exec_stackless_coroutine, destroy_stackless_coroutine, NULL, NULL, &attr);
    ffrt_submit_coroutine((void *)&co2, exec_stackless_coroutine, destroy_stackless_coroutine, NULL, NULL, &attr);
    // ffrt_poller_wakeup_001
    ffrt_poller_wakeup(ffrt_qos_default);
    usleep(100000);
    EXPECT_EQ(co1.count, 4);
    EXPECT_EQ(co2.count, 4);

    ffrt_wake_coroutine(nullptr);
}

HWTEST_F(CoroutineTest, coroutine_submit_fail, TestSize.Level0)
{
    // get_task_001
    EXPECT_EQ(ffrt_get_current_task(), nullptr);

    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);

    // coroutine_submit_001
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
    printf("cb done\n");
}

/*
* 测试用例名称：ffrt_get_current_coroutine_stack_success
* 测试用例描述：ffrt_get_current_coroutine_stack获取当前协程栈成功
* 预置条件    ：提交ffrt任务
* 操作步骤    ：在ffrt任务中调用ffrt_get_current_coroutine_stack接口
* 预期结果    ：获取协程栈地址和大小成功
*/
HWTEST_F(CoroutineTest, ffrt_get_current_coroutine_stack_success, TestSize.Level0)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    std::function<void()>&& OnePlusFunc = [&]() {
        void* stackAddr = nullptr;
        size_t size = 0;
        bool ret = ffrt_get_current_coroutine_stack(&stackAddr, &size);
        EXPECT_EQ(ret, true);
        EXPECT_FALSE(stackAddr == nullptr);
        EXPECT_NE(size, 0);
    };
    ffrt_task_handle_t task = ffrt_queue_submit_h(queue_handle,
        ffrt::create_function_wrapper(OnePlusFunc, ffrt_function_kind_queue), nullptr);
    const std::vector<ffrt_dependence_t> wait_deps = {{ffrt_dependence_task, task}};
    ffrt_deps_t wait{static_cast<uint32_t>(wait_deps.size()), wait_deps.data()};
    ffrt_wait_deps(&wait);
    ffrt_task_handle_destroy(task);
    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destroy(queue_handle);
}
