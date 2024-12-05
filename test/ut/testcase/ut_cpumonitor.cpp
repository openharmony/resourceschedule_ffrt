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
#define private public
#define protected public
#include <eu/scpu_monitor.h>
#include <util/worker_monitor.h>
#undef private
#undef protected

#include "qos.h"
#include "../common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;
using ::testing::Return;

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

HWTEST_F(CpuMonitorTest, monitor_notify_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    QoS qos(qos_default);
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1),
        CPUMonitor::HandleTaskNotifyDefault});

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

HWTEST_F(CpuMonitorTest, set_worker_max_num_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });

    int ret = wmonitor.SetWorkerMaxNum(qos(2), 160);
    wmonitor.WakeupSleep(QoS(5), true);
    wmonitor.RollbackDestroy(QoS(5), false);

    EXPECT_EQ(ret, -1);
}

HWTEST_F(CpuMonitorTest, set_worker_max_num_test2, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });
    
    int ret = wmonitor.SetWorkerMaxNum(qos(2), 0);
    wmonitor.WakeupSleep(QoS(5), true);
    wmonitor.RollbackDestroy(QoS(5), false);

    EXPECT_EQ(ret, -1);
}

HWTEST_F(CpuMonitorTest, total_count_test, TestSize.Level1)
{
    testing::NiceMock<MockWorkerManager> mWmanager;
    SCPUMonitor wmonitor({
        std::bind(&MockWorkerManager::IncWorker, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::WakeupWorkers, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetTaskCount, &mWmanager, std::placeholders::_1),
        std::bind(&MockWorkerManager::GetWorkerCount, &mWmanager, std::placeholders::_1) });
    
    int ret = monitor.TotalCount(qos(2));

    EXPECT_NE(ret, -1);
}

#endif