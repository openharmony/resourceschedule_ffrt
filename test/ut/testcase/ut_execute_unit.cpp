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

#include <random>

#include <gtest/gtest.h>

#define private public
#define protected public

#include "ffrt_inner.h"
#include "ffrt.h"
#include "tm/scpu_task.h"
#include "eu/sexecute_unit.h"
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
* 测试用例名称：ffrt_worker_escape
* 测试用例描述：ffrt_worker_escape接口测试
* 预置条件    ：无
* 操作步骤    ：调用enable和disable接口
* 预期结果    ：正常参数enable成功，非法参数或者重复调用enable失败
*/
HWTEST_F(ExecuteUnitTest, ffrt_worker_escape, TestSize.Level1)
{
    EXPECT_EQ(ffrt::enable_worker_escape(0, 0, 0, 0, 0), 1);
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
HWTEST_F(ExecuteUnitTest, notify_workers, TestSize.Level1)
{
    constexpr int count = 5;
    for (int i = 0; i < count; i++) {
        ffrt::submit([&]() {});
    }
    sleep(1);
    ffrt::notify_workers(2, 6);
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
    auto manager = std::make_unique<ffrt::SExecuteUnit>();
    manager->handleTaskNotify = ffrt::SExecuteUnit::HandleTaskNotifyConservative;

    auto task = std::make_unique<ffrt::SCPUEUTask>(nullptr, nullptr, 0);
    ffrt::Scheduler* sch = ffrt::Scheduler::Instance();
    sch->PushTask(ffrt::QoS(2), task.get());

    int executingNum = manager->GetWorkerGroup(ffrt::QoS(2)).executingNum;
    manager->GetWorkerGroup(2).executingNum = 20;
    manager->NotifyTask<ffrt::TaskNotifyType::TASK_ADDED>(ffrt::QoS(2));
    manager->NotifyTask<ffrt::TaskNotifyType::TASK_PICKED>(ffrt::QoS(2));
    manager->GetWorkerGroup(ffrt::QoS(2)).executingNum = executingNum;
    sch->PopTask(ffrt::QoS(2));
}

/*
* 测试用例名称：ffrt_handle_task_notify_ultra_conservative
* 测试用例描述：ffrt特别保守调度策略
* 预置条件    ：创建SExecuteUnit，策略设置为HandleTaskNotifyUltraConservative
* 操作步骤    ：调用SExecuteUnit的Notify方法
* 预期结果    ：成功执行HandleTaskNotifyUltraConservative方法
*/
HWTEST_F(ExecuteUnitTest, ffrt_handle_task_notify_ultra_conservative, TestSize.Level1)
{
    auto manager = std::make_unique<ffrt::SExecuteUnit>();
    manager->handleTaskNotify = ffrt::SExecuteUnit::HandleTaskNotifyUltraConservative;

    auto task = std::make_unique<ffrt::SCPUEUTask>(nullptr, nullptr, 0);
    ffrt::Scheduler* sch = ffrt::Scheduler::Instance();
    sch->PushTask(ffrt::QoS(2), task.get());

    int executingNum = manager->GetWorkerGroup(2).executingNum;
    manager->GetWorkerGroup(ffrt::QoS(2)).executingNum = 20;
    manager->NotifyTask<ffrt::TaskNotifyType::TASK_ADDED>(ffrt::QoS(2));
    manager->NotifyTask<ffrt::TaskNotifyType::TASK_PICKED>(ffrt::QoS(2));
    manager->GetWorkerGroup(ffrt::QoS(2)).executingNum = executingNum;
    sch->PopTask(ffrt::QoS(2));
}
/*
* 测试用例名称：ffrt_escape_submit_execute
* 测试用例描述：调用EU的逃生函数
* 预置条件    ：创建SExecuteUnit
* 操作步骤    ：调用ExecuteEscape、SubmitEscape、ReportEscapeEvent，包括异常分支
* 预期结果    ：成功执行ExecuteEscape、SubmitEscape、ReportEscapeEvent方法
*/
HWTEST_F(ExecuteUnitTest, ffrt_escape_submit_execute, TestSize.Level1)
{
    constexpr int taskCount = 100;
    auto manager = std::make_unique<ffrt::SExecuteUnit>();
    EXPECT_EQ(manager->SetEscapeEnable(10, 100, 1000, 0, 30), 0);
    for (int i = 0; i < taskCount; i++) {
        ffrt::submit([&]() {
            usleep(1000);
        });
    }
    manager->ExecuteEscape(qos_default);
    manager->SubmitEscape(qos_default, 1);
    manager->ReportEscapeEvent(qos_default, 1);

    ffrt::wait();
}

/*
* 测试用例名称：ffrt_inc_worker_abnormal
* 测试用例描述：调用EU的IncWorker函数
* 预置条件    ：创建SExecuteUnit
* 操作步骤    ：1.调用方法IncWorker，传入异常参数
               2.设置tearDown为true，调用IncWorker
* 预期结果    ：返回false
*/
HWTEST_F(ExecuteUnitTest, ffrt_inc_worker_abnormal, TestSize.Level1)
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
HWTEST_F(ExecuteUnitTest, BindWG, TestSize.Level1)
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
HWTEST_F(ExecuteUnitTest, UnbindTG, TestSize.Level1)
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
HWTEST_F(ExecuteUnitTest, BindTG, TestSize.Level1)
{
    auto qos1 = std::make_unique<QoS>();
    ThreadGroup* it = FFRTFacade::GetEUInstance().BindTG(*qos1);
    EXPECT_EQ(*qos1, qos_default);
}
