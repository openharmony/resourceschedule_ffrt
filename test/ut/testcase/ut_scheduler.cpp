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

/*
 * 测试用例名称：ffrt_sched_local_push_pop
 * 测试用例描述：TaskScheduler, 开启本地队列后push和pop
 * 预置条件    ：无
 * 操作步骤    ：1、初始化STaskScheduler
                 2、push和pop任务
 * 预期结果    ：push和pop后的任务数量符合预期
 */
HWTEST_F(SchedulerTest, ffrt_sched_local_push_pop, TestSize.Level0)
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
                 2、push任务到localQueue后保存到本地队列容器中
                 3、调用PopTask触发偷取任务
 * 预期结果    ：偷取任务成功
 */
HWTEST_F(SchedulerTest, ffrt_sched_local_priority, TestSize.Level0)
{
    STaskScheduler scheduler;
    scheduler.SetTaskSchedMode(TaskSchedMode::LOCAL_TASK_SCHED_MODE);
    constexpr int taskCount = 10;
    for (int i = 0; i < taskCount; i++) {
        TaskBase* task = new SCPUEUTask(nullptr, nullptr, 0);
        /* Note that currently there is only one priority slot.
         * Hence, there can be at most one task pushed.
         */
        auto pushed = scheduler.PushTaskToPriorityStack(task);
        if (!pushed) {
            delete task;
        }
    }

    auto task = scheduler.PopTask();
    EXPECT_NE(task, nullptr);

    while (task != nullptr) {
        delete task;
        task = scheduler.PopTask();
    }
    EXPECT_EQ(scheduler.GetLocalTaskCnt(), 0);
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

HWTEST_F(SchedulerTest, sched_test, TestSize.Level1)
{
    auto scheduler = std::make_unique<STaskScheduler>();
    scheduler->SetTaskSchedMode(TaskSchedMode::LOCAL_TASK_SCHED_MODE);
    ffrt_executor_task_t work = {};
    UVTask uvTask(&work, nullptr);
    scheduler->PushTask(&uvTask);
    bool ret = scheduler->CancelUVWork(&work);
#ifndef FFRT_GITEE
    EXPECT_EQ(ret, true);
#endif
    ffrt::TaskBase* task = new ffrt::SCPUEUTask(nullptr, nullptr, 0);
    scheduler->PushTask(task);
    scheduler->GetPriorityTaskCnt();
    scheduler->PushTaskLocalOrPriority(task);
}

/*
 * 测试用例名称：priority_task_0001
 * 测试用例描述：测试优先槽任务自己提交自己会不会导致其他任务不执行
 * 预置条件    ：NA
 * 操作步骤    ：1、设置TaskScheduler开启本地队列模式
                2、提交一个普通任务，不创建worker
                3、提交一个自己提交自己的优先槽任务，不唤醒worker
                4、通过notify，创建一个worker
 * 预期结果    ：不会导致其他任务饿死
 */
typedef struct {
    std::atomic<int> count;
    std::mutex lock;
} StacklessCoroutine1;

ffrt_coroutine_ret_t stackless_coroutine_3(void *co)
{
    static_cast<void*>(co);
    ffrt::TaskBase* wakedTask = static_cast<ffrt::TaskBase*>(ffrt_get_current_task());
    FFRTFacade::GetSchedInstance()->GetScheduler(2).PushTaskToPriorityStack(wakedTask);
    return ffrt_coroutine_pending;
}

ffrt_coroutine_ret_t exec_stackless_coroutine_3(void *co)
{
    static_cast<void*>(co);
    return stackless_coroutine_3(co);
}

void DestoryStacklessCoroutine3(void *co)
{
    static_cast<void*>(co);
}

HWTEST_F(SchedulerTest, priority_task_0001, TestSize.Level0)
{
    ffrt_set_cpu_worker_max_num(ffrt::QoS(2), 1);
    ffrt::Scheduler* scheduler = ffrt::FFRTFacade::GetSchedInstance();
    ffrt::TaskScheduler& taskScheduler = scheduler->GetScheduler(ffrt::QoS(2));
    taskScheduler.taskSchedMode = TaskSchedMode::LOCAL_TASK_SCHED_MODE;

    StacklessCoroutine1 co1 = {0};

    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_name(&attr, "stackless_coroutine_3");

    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(static_cast<ffrt_task_attr_t *>(&attr));
    ffrt::ffrt_io_callable_t work;
    work.exec = exec_stackless_coroutine_3;
    work.destroy = DestoryStacklessCoroutine3;
    work.data = &co1;
    IOTask* ioTask = TaskFactory<IOTask>::Alloc();
    new (ioTask) IOTask(work, p);

    int result = 0;
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr); // 初始化task属性，必须
    ffrt_task_attr_set_notify_worker(&task_attr, false);

    double t;
    std::function<void()>&& OnePlusFunc = [&result]() { result += 1; };
    std::function<void()>&& SetPriorityPtrFunc = [&]() {
        *(FFRTFacade::GetSchedInstance()->GetScheduler(2).GetPriorityTask()) = ioTask;
    };

    ffrt_task_handle_t oneTask = ffrt_submit_h_base(
        ffrt::create_function_wrapper(SetPriorityPtrFunc), {}, {}, &task_attr);
    ffrt_task_handle_t towTask = ffrt_submit_h_base(ffrt::create_function_wrapper(OnePlusFunc), {}, {}, &task_attr);

    ffrt_notify_workers(ffrt_qos_default, 1);
    const std::vector<ffrt_dependence_t> wait_deps = {{ffrt_dependence_task, towTask}};
    ffrt_deps_t wait{static_cast<uint32_t>(wait_deps.size()), wait_deps.data()};
    ffrt_wait_deps(&wait);

    EXPECT_EQ(result, 1);
    ffrt_task_attr_destroy(&task_attr);
    ffrt_task_handle_destroy(oneTask);
    ffrt_task_handle_destroy(towTask);
    taskScheduler.taskSchedMode = TaskSchedMode::DEFAULT_TASK_SCHED_MODE;
}

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