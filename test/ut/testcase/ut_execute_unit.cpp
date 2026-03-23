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

#define private public
#define protected public

#include "ffrt_inner.h"
#include "ffrt.h"
#include "tm/task_base.h"
#include "tm/scpu_task.h"
#include "eu/sexecute_unit.h"
#include "sched/stask_scheduler.h"
#include "util/capability.h"
#include "util/worker_monitor.h"
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class ExecuteUnitTest : public testing::Test {
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

/*
 * 测试用例名称：coroutine_release_task_test
 * 测试用例描述：超时任务队列清理操作
 * 预置条件    ：无
 * 操作步骤    ：1.向超时任务队列插入两条测试记录
                2.调用WorkerMonitor的CheckTaskStatus方法清理超时任务队列
 * 预期结果    ：此时处于无线程无活跃任务状态，超时任务队列会被清空
 */
HWTEST_F(ExecuteUnitTest, coroutine_release_task_test, TestSize.Level1)
{
    FFRTFacade::GetWMInstance().taskTimeoutInfo_.push_back({1, "test1"});
    FFRTFacade::GetWMInstance().taskTimeoutInfo_.push_back({2, "test2"});
    FFRTFacade::GetWMInstance().CheckTaskStatus();
    int x = FFRTFacade::GetWMInstance().taskTimeoutInfo_.size();
    EXPECT_EQ(x, 0);
}

/*
 * 测试用例名称：coroutine_release_task_test
 * 测试用例描述：无效线程状态清理操作
 * 预置条件    ：无
 * 操作步骤    ：1.提交一个耗时2s的任务
                2.在线程工作时检测线程状态队列大小
                3.在线程完成工作后再次检测线程状态队列大小
 * 预期结果    ：完成工作后无效线程状态会被清空
 */
HWTEST_F(ExecuteUnitTest, coroutine_release_worker_test, TestSize.Level1)
{
    ffrt::submit([&]() {
        usleep(2000000);
    });
    usleep(1000000);
    FFRTFacade::GetWMInstance().CheckWorkerStatus();
    int x1 = FFRTFacade::GetWMInstance().workerStatus_.size();
    EXPECT_NE(x1, 0);

    ffrt::wait();
    usleep(2000000);
    FFRTFacade::GetWMInstance().CheckWorkerStatus();
    int x2 = FFRTFacade::GetWMInstance().workerStatus_.size();
    EXPECT_GE(x2, 0);
}

/*
 * 测试用例名称：ffrt_worker_escape
 * 测试用例描述：ffrt_worker_escape接口测试
 * 预置条件    ：无
 * 操作步骤    ：调用enable和disable接口
 * 预期结果    ：正常参数enable成功，非法参数或者重复调用enable失败
 */
HWTEST_F(ExecuteUnitTest, ffrt_worker_escape, TestSize.Level0)
{
    EXPECT_EQ(ffrt::enable_worker_escape(0, 0, 0, 0, 0), 1);
    EXPECT_EQ(ffrt::enable_worker_escape(10, 0, 0, 0, 0), 1);
    EXPECT_EQ(ffrt::enable_worker_escape(10, 100, 0, 0, 0), 1);
    EXPECT_EQ(ffrt::enable_worker_escape(10, 100, 1000, 10, 0), 1);
    EXPECT_EQ(ffrt::enable_worker_escape(), 0);
    EXPECT_EQ(ffrt::enable_worker_escape(), 1);
    ffrt::disable_worker_escape();
}

/*
 * 测试用例名称：notify_workers
 * 测试用例描述：notify_workers接口测试
 * 预置条件    ：无
 * 操作步骤    ：1.提交5个任务，执行完等待worker休眠
                2.调用notify_workers接口，传入number为6
 * 预期结果    ：接口调用成功
 */
HWTEST_F(ExecuteUnitTest, notify_workers, TestSize.Level0)
{
    constexpr int count = 5;
    std::atomic_int number = 0;
    for (int i = 0; i < count; i++) {
        ffrt::submit([&]() {
            number++;
        });
    }
    sleep(1);
    ffrt::notify_workers(2, 6);
    EXPECT_EQ(count, number);
}

/*
 * 测试用例名称：ffrt_escape_submit_execute
 * 测试用例描述：调用EU的逃生函数
 * 预置条件    ：创建SExecuteUnit
 * 操作步骤    ：调用ExecuteEscape、SubmitEscape、ReportEscapeEvent，包括异常分支
 * 预期结果    ：成功执行ExecuteEscape、SubmitEscape、ReportEscapeEvent方法
 */
HWTEST_F(ExecuteUnitTest, ffrt_escape_submit_execute, TestSize.Level0)
{
    auto manager = std::make_unique<ffrt::SExecuteUnit>();
    EXPECT_EQ(manager->SetEscapeEnable(10, 100, 1000, 0, 30), 0);
    manager->ExecuteEscape(qos_default);
    manager->SubmitEscape(qos_default, 1);
    manager->SubmitEscape(qos_default, 1);
    manager->ReportEscapeEvent(qos_default, 1);
}

/*
* 测试用例名称：ffrt_inc_worker_abnormal
* 测试用例描述：调用EU的IncWorker函数
* 预置条件    ：创建SExecuteUnit
* 操作步骤    ：1.调用方法IncWorker，传入异常参数
               2.设置tearDown为true，调用IncWorker
* 预期结果    ：返回false
*/
HWTEST_F(ExecuteUnitTest, ffrt_inc_worker_abnormal, TestSize.Level0)
{
    auto manager = std::make_unique<ffrt::SExecuteUnit>();
    EXPECT_EQ(manager->IncWorker(QoS(-1)), false);
    manager->tearDown = true;
    EXPECT_EQ(manager->IncWorker(QoS(qos_default)), false);
}

/**
 * @tc.name: BindWG
 * @tc.desc: Test whether the BindWG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, BindWG, TestSize.Level0)
{
    auto qos1 = std::make_unique<QoS>();
    FFRTFacade::GetEUInstance().BindWG(*qos1);
    EXPECT_EQ(*qos1, qos_default);
}

/**
 * @tc.name: UnbindTG
 * @tc.desc: Test whether the UnbindTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, UnbindTG, TestSize.Level0)
{
    auto qos1 = std::make_unique<QoS>();
    FFRTFacade::GetEUInstance().UnbindTG(*qos1);
    EXPECT_EQ(*qos1, qos_default);
}

/**
 * @tc.name: BindTG
 * @tc.desc: Test whether the BindTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, BindTG, TestSize.Level0)
{
    auto qos1 = std::make_unique<QoS>();
    ThreadGroup* it = FFRTFacade::GetEUInstance().BindTG(*qos1);
    EXPECT_EQ(*qos1, qos_default);
}

HWTEST_F(ExecuteUnitTest, WorkerShare, TestSize.Level0)
{
    std::atomic<bool> done = false;
    CpuWorkerOps ops{
        [](CPUWorker* thread) { return WorkerAction::RETIRE; },
        [&done](CPUWorker* thread) {
            // prevent thread leak and UAF
            // by sync. via done and detaching the thread
            thread->SetExited();
            thread->Detach();
            done = true;
        },
        [](CPUWorker* thread) {},
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        []() { return false; },
#endif
    };

    const auto qos = QoS(5);
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(qos);
    workerCtrl.workerShareConfig.push_back({qos, true});
    auto worker = std::make_unique<CPUWorker>(qos, std::move(ops), 0);

    std::function<bool(int, CPUWorker*)> trueFunc = [](int qos, CPUWorker* worker) { return true; };
    std::function<bool(int, CPUWorker*)> falseFunc = [](int qos, CPUWorker* worker) { return false; };

#ifndef FFRT_GITEE
    EXPECT_EQ(manager->WorkerShare(worker.get(), trueFunc), true);
    EXPECT_EQ(manager->WorkerShare(worker.get(), falseFunc), false);
#endif

    workerCtrl.workerShareConfig[0].second = false;
    EXPECT_EQ(manager->WorkerShare(worker.get(), trueFunc), true);
    EXPECT_EQ(manager->WorkerShare(worker.get(), falseFunc), false);
    while (!done) {
        // busy wait for the worker thread to be done.
        // delay the destruction of main thread till the retirement of the worker.
    }
}

HWTEST_F(ExecuteUnitTest, HandleTaskNotifyConservative, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.executingNum, 0);

    SExecuteUnit::HandleTaskNotifyConservative(manager.get(), 5, TaskNotifyType::TASK_ADDED);
    SExecuteUnit::HandleTaskNotifyUltraConservative(manager.get(), 5, TaskNotifyType::TASK_ADDED);
    EXPECT_EQ(workerCtrl.executingNum, 0);

    workerCtrl.sleepingNum++;
    workerCtrl.executingNum = workerCtrl.maxConcurrency;
    manager->PokeImpl(5, 1, TaskNotifyType::TASK_ADDED);
    EXPECT_EQ(workerCtrl.executingNum, workerCtrl.maxConcurrency);

    workerCtrl.sleepingNum = 0;
    manager->PokeImpl(5, 1, TaskNotifyType::TASK_ADDED);
    EXPECT_EQ(workerCtrl.executingNum, workerCtrl.maxConcurrency);

    workerCtrl.maxConcurrency = 20;
    workerCtrl.executingNum = workerCtrl.hardLimit;
    manager->PokeImpl(5, 1, TaskNotifyType::TASK_ADDED);
    EXPECT_EQ(workerCtrl.executingNum, workerCtrl.hardLimit);

    if (manager->we_[0] != nullptr) {
        delete manager->we_[0];
        manager->we_[0] = nullptr;
    }
}

HWTEST_F(ExecuteUnitTest, SetWorkerStackSize, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);

    manager->SetWorkerStackSize(5, 4096);
    EXPECT_EQ(workerCtrl.workerStackSize, 4096);
}

HWTEST_F(ExecuteUnitTest, WorkerCreate, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.executingNum, 0);
    workerCtrl.WorkerCreate();
    EXPECT_EQ(workerCtrl.executingNum, 1);
}

HWTEST_F(ExecuteUnitTest, RollBackCreate, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.executingNum, 0);
    workerCtrl.RollBackCreate();
    EXPECT_EQ(workerCtrl.executingNum, -1);
}

/**
 * @tc.name: IntoSleep
 * @tc.desc: Test whether the IntoSleep interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, IntoSleep, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.executingNum, 0);
    EXPECT_EQ(workerCtrl.sleepingNum, 0);

    manager->IntoSleep(QoS(5));

    EXPECT_EQ(workerCtrl.executingNum, -1);
    EXPECT_EQ(workerCtrl.sleepingNum, 1);
}

/**
 * @tc.name: OutOfSleep
 * @tc.desc: Test whether the OutOfSleep interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, OutOfSleep, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.executingNum, 0);
    EXPECT_EQ(workerCtrl.sleepingNum, 0);

    workerCtrl.OutOfSleep(QoS(5));

    EXPECT_EQ(workerCtrl.executingNum, 1);
    EXPECT_EQ(workerCtrl.sleepingNum, -1);
}

/**
 * @tc.name: WorkerDestroy
 * @tc.desc: Test whether the WorkerDestroy interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, WorkerDestroy, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.sleepingNum, 0);

    workerCtrl.WorkerDestroy();

    EXPECT_EQ(workerCtrl.sleepingNum, -1);
}

/**
 * @tc.name: IntoDeepSleep
 * @tc.desc: Test whether the IntoDeepSleep interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(ExecuteUnitTest, IntoDeepSleep, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, 0);

    workerCtrl.IntoDeepSleep();

    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, 1);
}

HWTEST_F(ExecuteUnitTest, OutOfDeepSleep, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.sleepingNum, 0);
    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, 0);
    EXPECT_EQ(workerCtrl.executingNum, 0);

    workerCtrl.OutOfDeepSleep(QoS(5));

    EXPECT_EQ(workerCtrl.sleepingNum, -1);
    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, -1);
    EXPECT_EQ(workerCtrl.executingNum, 1);
}

HWTEST_F(ExecuteUnitTest, TryDestroy, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.sleepingNum, 0);

    bool res = workerCtrl.TryDestroy();

    EXPECT_EQ(false, res);
}

HWTEST_F(ExecuteUnitTest, RollbackDestroy, TestSize.Level0)
{
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(5);
    EXPECT_EQ(workerCtrl.executingNum, 0);

    workerCtrl.RollbackDestroy();

    EXPECT_EQ(workerCtrl.executingNum, 1);
}

HWTEST_F(ExecuteUnitTest, SetCgroupAttr, TestSize.Level0)
{
    ffrt_os_sched_attr attr2 = {100, 19, 0, 10, 0, "0-6"};
    EXPECT_EQ(ffrt::set_cgroup_attr(static_cast<int>(ffrt::qos_user_interactive), &attr2), 0);
    ffrt::restore_qos_config();
}

/*
* 测试用例名称：ffrt_disable_worker_monitor
* 测试用例描述：调用EU的DisableWorkerMonitor函数
* 预置条件    ：创建SExecuteUnit
* 操作步骤    ：1.调用方法DisableWorkerMonitor，传入异常参数
* 预期结果    ：monitor相关参数被设置为false
*/
HWTEST_F(ExecuteUnitTest, ffrt_disable_worker_monitor, TestSize.Level1)
{
    std::atomic<bool> done = false;
    CpuWorkerOps ops {
        [](CPUWorker* thread) { return WorkerAction::RETIRE; },
        [&done](CPUWorker* thread) {
            // prevent thread leak and UAF
            // by sync. via done and detaching the thread
            thread->SetExited();
            thread->Detach();
            done = true;
        },
        [](CPUWorker* thread) {},
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        []() { return false; },
#endif
    };

    const auto qos = QoS(5);
    auto manager = std::make_unique<SExecuteUnit>();
    CPUWorkerGroup& workerCtrl = manager->GetWorkerGroup(qos);
    CPUWorker* worker = new CPUWorker(qos, std::move(ops), 0);
    workerCtrl.threads[worker] = std::unique_ptr<CPUWorker>(worker);
    EXPECT_TRUE(worker->monitor_);

    manager->DisableWorkerMonitor(qos, worker->Id());
    EXPECT_FALSE(worker->monitor_);

    while (!done) {
        // busy wait for the worker thread to be done.
        // delay the destruction of main thread till the retirement of the worker.
    }
}

/*
 * 测试用例名称：ffrt_handle_task_notify_conservative
 * 测试用例描述：ffrt保守调度策略
 * 预置条件    ：创建SCPUWorkerManager，策略设置为HandleTaskNotifyConservative
 * 操作步骤    ：调用SCPUWorkerManager的Notify方法
 * 预期结果    ：成功执行HandleTaskNotifyConservative方法
 */
HWTEST_F(ExecuteUnitTest, ffrt_handle_task_notify_conservative, TestSize.Level1)
{
    ffrt::SExecuteUnit* manager = new ffrt::SExecuteUnit();
    manager->handleTaskNotify = ffrt::SExecuteUnit::HandleTaskNotifyConservative;

    ffrt::TaskBase* task = new ffrt::SCPUEUTask(nullptr, nullptr, 0);
    ffrt::Scheduler* sch = ffrt::Scheduler::Instance();
    sch->PushTask(ffrt::QoS(2), task);

    int executingNum = manager->GetWorkerGroup(ffrt::QoS(2)).executingNum;
    manager->GetWorkerGroup(2).executingNum = 20;
    manager->NotifyTask<ffrt::TaskNotifyType::TASK_PICKED>(ffrt::QoS(2));
    manager->GetWorkerGroup(ffrt::QoS(2)).executingNum = executingNum;
    sch->PopTask(ffrt::QoS(2));

    delete manager;
    delete task;
}

/*
 * 测试用例名称：ffrt_handle_task_notify_ultra_conservative
 * 测试用例描述：ffrt特别保守调度策略
 * 预置条件    ：创建SCPUWorkerManager，策略设置为HandleTaskNotifyUltraConservative
 * 操作步骤    ：调用SCPUWorkerManager的Notify方法
 * 预期结果    ：成功执行HandleTaskNotifyUltraConservative方法
 */
HWTEST_F(ExecuteUnitTest, ffrt_handle_task_notify_ultra_conservative, TestSize.Level1)
{
    ffrt::SExecuteUnit* manager = new ffrt::SExecuteUnit();
    manager->handleTaskNotify = ffrt::SExecuteUnit::HandleTaskNotifyUltraConservative;

    ffrt::TaskBase* task = new ffrt::SCPUEUTask(nullptr, nullptr, 0);
    ffrt::Scheduler* sch = ffrt::Scheduler::Instance();
    sch->PushTask(ffrt::QoS(2), task);

    int executingNum = manager->GetWorkerGroup(2).executingNum;
    manager->GetWorkerGroup(ffrt::QoS(2)).executingNum = 20;
    manager->NotifyTask<ffrt::TaskNotifyType::TASK_ADDED>(ffrt::QoS(2));
    manager->GetWorkerGroup(ffrt::QoS(2)).executingNum = executingNum;
    sch->PopTask(ffrt::QoS(2));

    delete manager;
    delete task;
}

/*
 * 测试用例名称：ffrt_task_get_tid_test
 * 测试用例描述：测试ffrt_task_get_tid接口
 * 预置条件    ：创建SCPUEUTask
 * 操作步骤    ：调用ffrt_task_get_tid方法，入参分别为SCPUEUTask对象和空指针
 * 预期结果    ：ffrt_task_get_tid功能正常，传入空指针时返回0
 */
HWTEST_F(ExecuteUnitTest, ffrt_task_get_tid_test, TestSize.Level1)
{
    ffrt::CPUEUTask* task = new ffrt::SCPUEUTask(nullptr, nullptr, 0);
    task->runningTid.store(pthread_self());
    pthread_t tid = ffrt_task_get_tid(task);
    EXPECT_EQ(tid, pthread_self());

    tid = ffrt_task_get_tid(nullptr);
    EXPECT_EQ(tid, 0);
}

/*
 * 测试用例名称：ffrt_set_sched_mode
 * 测试用例描述：ffrt_set_sched_mode EU调度模式设置
 * 预置条件    ：NA
 * 操作步骤    ：在非ffrt任务中调用ffrt_set_sched_mode接口
 * 预期结果    ：设置EU调度策略为默认模式、性能模式或节能模式
 */
HWTEST_F(ExecuteUnitTest, ffrt_set_sched_mode, TestSize.Level1)
{
    ffrt::sched_mode_type sched_type = ffrt::ExecuteUnit::Instance().GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_default_mode));

    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_energy_saving_mode);
    sched_type = ffrt::ExecuteUnit::Instance().GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_energy_saving_mode));

    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_performance_mode);
    sched_type = ffrt::ExecuteUnit::Instance().GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_performance_mode));
    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_default_mode);
}

/*
 * 测试用例名称：worker_escape_stage_one_report
 * 测试用例描述：触发ffrt一阶段逃生事件上报频率符合超过1s再上报
 * 预置条件    ：NA
 * 操作步骤    ：在ffrt任务中提交阻塞任务使ffrt创建超过16个worker
 * 预期结果    ：触发逃生且逃生事件上报时间更新
 */
HWTEST_F(ExecuteUnitTest, worker_escape_stage_one_report, TestSize.Level0)
{
    ffrt::disable_worker_escape();
    const int stageOneWorkerNum = 20; // 一阶段worker数量
    const int firstBatchNum = 17;    // 第一批任务数, 17个worker刚好触发逃生，新增1个worker
    const int secondBatchNum = 3;   // 第二批任务数
    bool blockTaskFlag = true;
    int completedCount = 0;
    std::mutex mtx;
    std::condition_variable cv;

    // escape使能并初始化
    int ret = ffrt::enable_worker_escape(10, 1000, 10000, stageOneWorkerNum, 1024);
    EXPECT_EQ(ret, 0);

    auto curTime = std::chrono::steady_clock::now();
    ffrt::CPUWorkerGroup& workerGroup = ffrt::FFRTFacade::GetEUInstance().GetWorkerGroup(ffrt_qos_default);

    auto submitTasks = [&](int taskNum) {
        for (int i = 0; i < taskNum; i++) {
            ffrt::submit([&] {
                std::unique_lock lock(mtx);
                completedCount++;
                if (blockTaskFlag) {
                    cv.wait(lock);
                }
            });
        }
    };

    // 提交第一批任务
    submitTasks(firstBatchNum);
    usleep(1000000);
    // 等待1s后提交第二批任务
    submitTasks(secondBatchNum);
    usleep(500000);
    {
        std::lock_guard lg(mtx);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        EXPECT_EQ(completedCount, stageOneWorkerNum); // 校验是否触发逃生
        EXPECT_TRUE(workerGroup.escapeReportTime >= curTime); // 校验逃生上报时间是否更新
#endif
        blockTaskFlag = false;
    }
    cv.notify_all();
    ffrt::wait();

    EXPECT_EQ(completedCount, stageOneWorkerNum);
    ffrt::disable_worker_escape();
}

/*
 * 测试用例名称：worker_escape_stage_two_report
 * 测试用例描述：触发ffrt二阶段逃生且逃生事件上报
 * 预置条件    ：NA
 * 操作步骤    ：在ffrt任务中提交阻塞任务使ffrt创建超过16个worker
 * 预期结果    ：触发逃生且逃生事件上报时间更新
 */
HWTEST_F(ExecuteUnitTest, worker_escape_stage_two_report, TestSize.Level0)
{
    ffrt::disable_worker_escape();
    const int totalTaskNum = 50;
    const int stageTwoWorkerNum = 30;
    bool blockTaskFlag = true;
    int completedCount = 0;
    std::mutex mtx;
    std::condition_variable cv;
    int ret = ffrt::enable_worker_escape(10000, 100, 10000, 0, stageTwoWorkerNum);
    EXPECT_EQ(ret, 0);

    ffrt::CPUWorkerGroup& workerGroup = ffrt::FFRTFacade::GetEUInstance().GetWorkerGroup(ffrt_qos_default);
    auto curTime = std::chrono::steady_clock::now();

    // 提交任务触发二阶段逃生
    for (int i = 0; i < totalTaskNum; i++) {
        ffrt::submit([&] {
            std::unique_lock lock(mtx);
            completedCount++;
            if (blockTaskFlag) {
                cv.wait(lock);
            }
        });
    }
    usleep(1500000);
    {
        std::lock_guard lg(mtx);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        EXPECT_GE(completedCount, stageTwoWorkerNum);
        EXPECT_TRUE(workerGroup.escapeReportTime >= curTime); // 校验逃生上报时间是否更新
#endif
        blockTaskFlag = false;
    }
    cv.notify_all();
    ffrt::wait();

    EXPECT_EQ(completedCount, totalTaskNum);
    ffrt::disable_worker_escape();
}
