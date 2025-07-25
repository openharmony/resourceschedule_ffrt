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
#include "sched/stask_scheduler.h"
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
    ffrt::FIFOQueue *fifoqueue = new ffrt::FIFOQueue();
    int aimnum = 10;
    SCPUEUTask task(nullptr, nullptr, 0);
    for (int i = 0; i < aimnum ; i++) {
        fifoqueue->EnQueue(&task);
    }
    EXPECT_EQ(fifoqueue->Size(), aimnum);
    EXPECT_EQ(fifoqueue->Empty(), false);
    delete fifoqueue;
}

/*
 * 测试用例名称：ffrt_sched_local_push_pop
 * 测试用例描述：TaskScheduler, 开启本地队列后push和pop
 * 预置条件    ：无
 * 操作步骤    ：1、初始化STaskScheduler
 *              2、push和pop任务
 * 预期结果    ：push和pop后的任务数量符合预期
 */
HWTEST_F(SchedulerTest, ffrt_sched_local_push_pop, TestSize.Level1)
{
    STaskScheduler scheduler;
    scheduler.SetTaskSchedMode(TaskSchedMode::LOCAL_TASK_SCHED_MODE);

    int taskCount = 100;

    EXPECT_EQ(scheduler.GetGlobalTaskCnt(), 0);

    task_attr_private attr;
    attr.notifyWorker_ = false;
    for (int i = 0; i < taskCount; i++) {
        TaskBase* task = new SCPUEUTask(&attr, nullptr, 0);
        scheduler.PushTaskGlobal(task, false);
    }

    EXPECT_EQ(scheduler.GetTotalTaskCnt(), taskCount);

    // 提交三个任务，保持worker不退出1秒
    for (int i = 0; i < 3; i++) {
        submit([]() {
            sleep(1);
        });
    }

    // 赋值tick，保证能够从全局队列拿任务
    unsigned int tick = 0;
    *(scheduler.GetWorkerTick()) = &tick;

    TaskBase* task = scheduler.PopTask();

    while (task != nullptr) {
        delete task;
        task = scheduler.PopTask();
    }

    EXPECT_EQ(scheduler.GetLocalTaskCnt(), 0);

    ffrt::wait();
}

/*
 * 测试用例名称 : ffrt_sched_local_steal
 * 测试用例描述 : TaskSchduler, 开启本地队列后偷任务
 * 预置条件    ：无
 * 操作步骤     : 1.初始化STaskScheduler
 *               2.push任务到localQueue后保存到本地队列容器中
 *               3.调用PopTask触发偷取任务
 * 预期结果    : 偷取任务成功
 */
HWTEST_F(SchedulerTest, ffrt_sched_local_steal, TestSize.Level0)
{
    STaskScheduler scheduler;
    scheduler.SetTaskSchedMode(TaskSchedMode::LOCAL_TASK_SCHED_MODE);

    int taskCount = 10;

    SpmcQueue localQueue;
    localQueue.Init(128);

    for (int i = 0; i < taskCount; i++) {
        TaskBase* task = new SCPUEUTask(nullptr, nullptr, 0);
        localQueue.PushTail(task);
    }

    scheduler.localQueues.emplace(syscall(SYS_gettid) + 1, &localQueue);

    EXPECT_GE(scheduler.GetTotalTaskCnt(), taskCount);

    EXPECT_GE(scheduler.GetPriorityTaskCnt(), 0);

    TaskBase* task = scheduler.PopTask();

    while (task != nullptr) {
        delete task;
        task = scheduler.PopTask();
    }

    EXPECT_EQ(scheduler.GetLocalTaskCnt(), 0);
}

/*
 * 测试用例名称：ffrt_sched_local_priority
 * 测试用例描述：TaskSchduler, 开启本地队列后向优先槽推任务
 * 预置条件    ：无
 * 操作步骤    ：1、初始化STaskScheduler
 *              2、push任务到localQueue后保存到本地队列容器中
 *              3、调用PopTask触发偷取任务
 * 预期结果    : 偷取任务成功
 */
HWTEST_F(SchedulerTest, ffrt_sched_local_priority, TestSize.Level0)
{
    STaskScheduler scheduler;
    scheduler.SetTaskSchedMode(TaskSchedMode::LOCAL_TASK_SCHED_MODE);

    int taskCount = 10;

    for (int i = 0; i < taskCount; i++) {
        TaskBase* task = new SCPUEUTask(nullptr, nullptr, 0);
        scheduler.PushTaskToPriorityStack(task);
    }

    auto task = scheduler.PopTask();

    EXPECT_NE(task, nullptr);

    while (task != nullptr) {
        delete task;
        task = scheduler.PopTask();
    }

    EXPECT_LE(scheduler.GetLocalTaskCnt(), 0);
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
 * 操作步骤    ：1、初始化两个队列， 一个填10个，一个空间未初始化
 *              2、调用PopHeadToAnotherQueue向另一个队列中push任务
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

    auto task = anotherQueue.PopHead();
    while (task != nullptr) {
        delete reinterpret_cast<TaskBase*>(task);
        task = anotherQueue.PopHead();
    }
}

HWTEST_F(SchedulerTest, sched_test, TestSize.Level0)
{
    STaskScheduler* scheduler = new STaskScheduler();
    scheduler->SetTaskSchedMode(TaskSchedMode::LOCAL_TASK_SCHED_MODE);
    ffrt_executor_task_t work = {};
    bool ret = scheduler->CancelUVWork(&work);
    EXPECT_EQ(ret, false);
    ffrt::TaskBase* task = new ffrt::SCPUEUTask(nullptr, nullptr, 0);
    scheduler->PushTask(task);
    scheduler->GetPriorityTaskCnt();
    scheduler->PushTaskLocalOrPriority(task);
    delete scheduler;
}
