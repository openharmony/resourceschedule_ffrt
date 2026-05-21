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

#include <list>
#include <vector>
#include <queue>
#include <thread>
#include <gtest/gtest.h>
#define private public
#define protected public
#include "ffrt_inner.h"

#include "core/entity.h"
#include "sched/task_scheduler.h"
#include "sched/scheduler.h"
#include "core/task_attr_private.h"
#include "tm/scpu_task.h"
#include "tm/task_base.h"
#include "tm/io_task.h"
#include "sched/stask_scheduler.h"
#include "util/ffrt_facade.h"
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class SchedulerTest : public testing::Test {
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

HWTEST_F(SchedulerTest, ffrt_task_runqueue_test, TestSize.Level0)
{
    auto fifoqueue = std::make_unique<ffrt::FIFOQueue>();
    constexpr int enqCount = 10;
    SCPUEUTask task(nullptr, nullptr, 0);
    for (int i = 0; i < enqCount ; i++) {
        fifoqueue->EnQueue(&task);
    }
    EXPECT_EQ(fifoqueue->Size(), enqCount);
    EXPECT_EQ(fifoqueue->Empty(), false);
}

namespace {
    void PushTest(void* task) {
        if (task != nullptr) {
            delete reinterpret_cast<TaskBase*>(task);
        }
    }
}

/*
 * 测试用例名称：task_runqueue_pop_to_another_full_test
 * 测试用例描述：SpmcQueue, 向另一个即将满任务的队列中push任务
 * 预置条件    ：无
 * 操作步骤    ：1、初始化两个队列，一个填10个，一个空间未初始化
                 2、调用PopHeadToAnotherQueue向另一个队列中push任务
 * 预期结果    ：Push任务失败，原队列任务数量符合预期
 */
HWTEST_F(SchedulerTest, task_runqueue_pop_to_another_fail_test, TestSize.Level0)
{
    int taskCount = 10;
    int localSize = 128;
    SpmcQueue localQueue;
    localQueue.Init(localSize);

    SpmcQueue anotherQueue;
    anotherQueue.tail_ = 0;
    anotherQueue.head_ = 0;
    anotherQueue.capacity_ = localSize;

    for (int i = 0; i < taskCount; i++) {
        TaskBase* task = new SCPUEUTask(nullptr, nullptr, 0);
        localQueue.PushTail(task);
    }

    int pushCount = localQueue.PopHeadToAnotherQueue(anotherQueue, taskCount, PushTest);

    EXPECT_EQ(localQueue.GetLength(), taskCount - 1);
    EXPECT_EQ(anotherQueue.GetLength(), 0);
    EXPECT_EQ(pushCount, 0);

    auto task = localQueue.PopHead();
    while (task != nullptr) {
        delete reinterpret_cast<TaskBase*>(task);
        task = localQueue.PopHead();
    }
}

typedef struct {
    std::atomic<int> count;
    std::mutex lock;
} StacklessCoroutine1;

/*
 * 测试用例名称：ffrt_task_handle_copy_test_001
 * 测试用例描述：ffrt的handle拷贝构造和拷贝赋值设置
 * 预置条件    ：NA
 * 操作步骤    ：在ffrt任务中调用拷贝构造和拷贝赋值接口
 * 预期结果    ：task_handle引用计数增减符合预期，task_handle析构符合预期
 */
HWTEST_F(SchedulerTest, ffrt_task_handle_copy_test_001, TestSize.Level0)
{
    int data = 0;
    int count = 1000;
    std::atomic_bool enable = true;

    ffrt::task_handle t1 = ffrt::submit_h([&]() {
        data += 2;
        while (enable) {
            usleep(10);
        }
    });
    ffrt_task_handle_t p1 = t1;
    auto task1 = static_cast<ffrt::CPUEUTask*>(p1);
    EXPECT_EQ(task1->rc.load(), 2); // 任务尚未结束，预期rc为2

    enable = false;

    ffrt::wait();

    // 任务完成以后，handle引用计数释放是异步流程，添加等待延时，等待引用计数减1
    while (task1->rc.load() != 1 && count > 0) {
        usleep(1000);
        count--;
    }

    EXPECT_GT(count, 0);
    EXPECT_EQ(task1->rc.load(), 1); // 任务已经结束，预期rc为1

    ffrt::task_handle t2 = t1; // 调用拷贝构造函数
    ffrt_task_handle_t p2 = t2;
    auto task2 = static_cast<ffrt::CPUEUTask*>(p2);
    EXPECT_EQ(task1->rc.load(), 2); // 拷贝构造函数调用成功，rc值加1
    EXPECT_EQ(task1->rc.load(), task2->rc.load());

    ffrt::task_handle t3 = nullptr;
    t3 = t2; // 调用拷贝赋值
    ffrt_task_handle_t p3 = t3;
    auto task3 = static_cast<ffrt::CPUEUTask*>(p3);
    EXPECT_EQ(task1->rc.load(), 3); // 拷贝赋值调用成功，rc值加1

    ffrt_task_handle_destroy(t1);
    EXPECT_EQ(task1->rc.load(), 2);
    ffrt_task_handle_destroy(t2);
    EXPECT_EQ(task2->rc.load(), 1);
    ffrt_task_handle_destroy(t3);
}