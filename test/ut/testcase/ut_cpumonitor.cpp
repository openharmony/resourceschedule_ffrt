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
#include <gmock/gmock.h>
#ifndef WITH_NO_MOCKER
#include <mockcpp/mockcpp.hpp>
#endif
#define private public
#define protected public
#include <eu/scpu_monitor.h>
#include <util/worker_monitor.h>
#undef private
#undef protected

#include "qos.h"
#include "../common.h"

#ifndef FFRT_GITEE
#include "cpp/ffrt_dynamic_graph.h"
#include "c/ffrt_dynamic_graph.h"
#endif

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;
using ::testing::Return;

#ifndef FFRT_GITEE
namespace {
// 返回当前环境是软化还是硬化
bool CheckSoftwareEnv()
{
    return (ffrt_hcs_get_capability(FFRT_HW_DYNAMIC_XPU_NORMAL) == 0) ? true : false;
}
}
#endif

class WorkerManager {
public:
    virtual bool IncWorker(const QoS& qos)
    {
        return true;
    }
    virtual void WakeupWorkers(const QoS& qos)
    {
    }
    virtual int GetTaskCount(const QoS& qos)
    {
        return 0;
    }
    virtual int GetWorkerCount(const QoS& qos)
    {
        return 0;
    }
};

class MockWorkerManager : public WorkerManager {
public:
    MockWorkerManager()
    {
    }
    ~MockWorkerManager()
    {
    }
    MOCK_METHOD1(IncWorker, bool(const QoS&));
    MOCK_METHOD1(WakeupWorkers, void(const QoS&));
    MOCK_METHOD1(GetTaskCount, int(const QoS&));
    MOCK_METHOD1(GetWorkerCount, int(const QoS&));
};

class CpuMonitorTest : public testing::Test {
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

/**
 * 测试用例名称：set_sched_mode
 * 测试用例描述：验证设置EU调度模式的接口
 * 预置条件：
 * 操作步骤：设置不同的调度模式，并通过get接口与设置值对比
 * 预期结果：设置成功
 */
#ifndef FFRT_GITEE
HWTEST_F(CpuMonitorTest, set_sched_mode, TestSize.Level1)
{
    if (CheckSoftwareEnv()) {
        return;
    }
    ffrt::sched_mode_type sched_type = ffrt::CPUManagerStrategy::GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_default_mode));

    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_energy_saving_mode);
    sched_type = ffrt::CPUManagerStrategy::GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_default_mode));

    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_performance_mode);
    sched_type = ffrt::CPUManagerStrategy::GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_performance_mode));

    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_default_mode);
}
#endif

HWTEST_F(CpuMonitorTest, monitor_notify_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    QoS qos(qos_default);
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1),
        SCPUMonitor::HandleTaskNotifyDefault});

    (void)wmonitor.GetMonitorTid();

    EXPECT_CALL(mWmanager, GetTaskCount(qos)).WillRepeatedly(Return(0));

    wmonitor.Notify(QoS(static_cast<int>(qos_default)), TaskNotifyType::TASK_ADDED);
    wmonitor.Notify(QoS(static_cast<int>(qos_default)), TaskNotifyType::TASK_PICKED);

    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, 0);
    EXPECT_CALL(mWmanager, GetTaskCount(qos)).WillRepeatedly(Return(1));
    EXPECT_CALL(mWmanager, GetWorkerCount(qos)).WillRepeatedly(Return(5));

    for (uint32_t idx = 1; idx <= 5; idx++) {
        wmonitor.Notify(QoS(static_cast<int>(qos_default)), TaskNotifyType::TASK_ADDED);
        EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, idx);
    }

    wmonitor.Notify(QoS(static_cast<int>(qos_default)), TaskNotifyType::TASK_PICKED);
    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, 5);
}

// 批量唤醒worker
HWTEST_F(CpuMonitorTest, monitor_notify_workers_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1)});

    QoS qosDefault(qos_default);
    wmonitor.NotifyWorkers(qosDefault, 3);
    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, 3);
    wmonitor.NotifyWorkers(qosDefault, 2);
    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, 5);
    wmonitor.NotifyWorkers(qosDefault, 5);
    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, 8);

    wmonitor.ctrlQueue[qos_default].executionNum = 4;
    wmonitor.ctrlQueue[qos_default].sleepingWorkerNum = 4;
    wmonitor.NotifyWorkers(qosDefault, 5);
    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, 4);

    notify_workers(qos_default, 1);
}

HWTEST_F(CpuMonitorTest, SubmitTask_test, TestSize.Level1)
{
    WorkerMonitor workermonitor;
    workermonitor.skipSampling_ = true;
    workermonitor.SubmitTask();

    EXPECT_EQ(workermonitor.skipSampling_, true);
}

HWTEST_F(CpuMonitorTest, SubmitMemReleaseTask_test, TestSize.Level1)
{
    WorkerMonitor workermonitor;
    workermonitor.skipSampling_ = true;
    workermonitor.SubmitMemReleaseTask();

    EXPECT_EQ(workermonitor.skipSampling_, true);
}

HWTEST_F(CpuMonitorTest, CheckWorkerStatus_test, TestSize.Level1)
{
    WorkerMonitor workermonitor;
    workermonitor.skipSampling_ = true;
    workermonitor.CheckWorkerStatus();

    EXPECT_EQ(workermonitor.skipSampling_, true);
}

/*
 * 测试用例名称 ；worker_escape_test
 * 测试用例描述 ：测试逃生惩罚功能
 * 预置条件     ：1、对CPUMonitor的IncWorker、WakeupWorkers、GetTaskCount方法进行打桩
 *               2、对CPUMonitor的GetRunningNum方法进行打桩，使其始终返回0
 *               3、将CPUMonitor的executionNum设置为16
 * 操作步骤     ：1、调用enable_worker_escape传入非法入参
 *               2、调用enable_worker_escape
 *               3、再次调用enable_worker_escape
 *               4、调用CPUMonitor的Poke方法
 * 预期结果     ：1、传入非法入参返回错误码
 *               2、重复调用返回错误码
 *               3、逃生惩罚功能生效
*/
HWTEST_F(CpuMonitorTest, worker_escape_test, TestSize.Level1)
{
#ifdef WITH_NO_MOCKER
    ffrt::disable_worker_escape();

    // 非法入参
    EXPECT_EQ(ffrt::enable_worker_escape(0), 1);
    EXPECT_EQ(ffrt::enable_worker_escape(), 0);
    // 不允许重复调用
    EXPECT_EQ(ffrt::enable_worker_escape(), 1);
#else
    int incWorkerNum = 0;
    int wakedWorkerNum = 0;
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        [&] (const QoS& qos) { incWorkerNum++; return true; },
        [&] (const QoS& qos) { wakedWorkerNum++; },
        [] (const QoS& qos) { return 1; },
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });
    MOCKER_CPP(&SCPUMonitor::GetRunningNum).stubs().will(returnValue(static_cast<size_t>(0)));

    ffrt::disable_worker_escape();
    // 非法入参
    EXPECT_EQ(ffrt::enable_worker_escape(0), 1);
    EXPECT_EQ(ffrt::enable_worker_escape(), 0);
    // 不允许重复调用
    EXPECT_EQ(ffrt::enable_worker_escape(), 1);

    WorkerCtrl& workerCtrl = wmonitor.ctrlQueue[2];
    workerCtrl.executionNum = 16;

    wmonitor.Poke(2, 1, TaskNotifyType::TASK_ADDED);
    usleep(100 * 1000);
    EXPECT_EQ(incWorkerNum, 1);

    wmonitor.ops.GetTaskCount = [&] (const QoS& qos) { workerCtrl.sleepingWorkerNum = 1; return 1; };
    wmonitor.Poke(2, 1, TaskNotifyType::TASK_ADDED);
    usleep(100 * 1000);
    EXPECT_EQ(wakedWorkerNum, 1);
#endif
}

#ifndef FFRT_GITEE
/**
 * @tc.name: TryDestroy
 * @tc.desc: Test whether the RollbackDestroy interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, monitor_worker_trydestroy_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });

    QoS qosDefault(qos_default);
    wmonitor.ctrlQueue[qos_default].sleepingWorkerNum = 1;
    wmonitor.TryDestroy(qosDefault);
    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].sleepingWorkerNum, 0);
}

/**
 * @tc.name: RollbackDestroy
 * @tc.desc: Test whether the RollbackDestroy interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, monitor_worker_rollbackdestroy_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });

    QoS qosDefault(qos_default);
    wmonitor.ctrlQueue[qos_default].executionNum = 0;
    wmonitor.RollbackDestroy(qosDefault, true);
    EXPECT_EQ(wmonitor.ctrlQueue[qos_default].executionNum, 1);
}

/*
 * 测试用例名称 ；set_worker_num_test
 * 测试用例描述 ：测试传入0-QOS_WORKER_MAXNUM之间worker数量的情况
 * 操作步骤     ：传入worker数量为4
 * 预期结果     ：预期成功
*/
HWTEST_F(CpuMonitorTest, set_worker_num_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });

    int ret = wmonitor.SetWorkerMaxNum(qos(2), 4);
    wmonitor.WakeupSleep(QoS(5), true);
    wmonitor.RollbackDestroy(QoS(5), false);

    EXPECT_EQ(ret, 0);
}

HWTEST_F(CpuMonitorTest, total_count_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });

    int ret = wmonitor.TotalCount(qos(2));

    EXPECT_NE(ret, -1);
}

#endif