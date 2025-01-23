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
#include <thread>
#define private public
#define protected public
#include "eu/cpu_worker.h"
#include "eu/scpuworker_manager.h"
#include "eu/scpu_monitor.h"
#include "eu/cpu_manager_strategy.h"
#include "eu/worker_thread.h"
#include "qos.h"
#include "common.h"

#undef private
#undef protected

namespace OHOS {
namespace FFRT_TEST {
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace OHOS::FFRT_TEST;
using namespace ffrt;
using namespace std;


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
 * @tc.name: IntoSleep
 * @tc.desc: Test whether the IntoSleep interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, IntoSleep, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    WorkerCtrl& workerCtrl = cpu.ctrlQueue[5];
    EXPECT_EQ(workerCtrl.executionNum, 0);
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 0);

    cpu.IntoSleep(QoS(5));

    EXPECT_EQ(workerCtrl.executionNum, -1);
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 1);
}

/**
 * @tc.name: WakeupSleep
 * @tc.desc: Test whether the WakeupSleep interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, WakeupSleep, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    EXPECT_NE(it, nullptr);

    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    WorkerCtrl& workerCtrl = cpu.ctrlQueue[5];
    EXPECT_EQ(workerCtrl.executionNum, 0);
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 0);

    cpu.WakeupSleep(QoS(5));

    EXPECT_EQ(workerCtrl.executionNum, 1);
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, -1);
}


/**
 * @tc.name: TimeoutCount
 * @tc.desc: Test whether the TimeoutCount interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, TimeoutCount, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    EXPECT_NE(it, nullptr);
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    WorkerCtrl& workerCtrl = cpu.ctrlQueue[5];
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 0);

    cpu.TimeoutCount(QoS(5));

    EXPECT_EQ(workerCtrl.sleepingWorkerNum, -1);
}

/**
 * @tc.name: IntoDeepSleep
 * @tc.desc: Test whether the IntoDeepSleep interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, IntoDeepSleep, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    EXPECT_NE(it, nullptr);
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    WorkerCtrl& workerCtrl = cpu.ctrlQueue[5];
    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, 0);

    cpu.IntoDeepSleep(QoS(5));

    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, 1);
}


HWTEST_F(CpuMonitorTest, WakeupDeepSleep, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    EXPECT_NE(it, nullptr);
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    WorkerCtrl& workerCtrl = cpu.ctrlQueue[5];
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 0);
    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, 0);
    EXPECT_EQ(workerCtrl.executionNum, 0);

    cpu.WakeupDeepSleep(QoS(5));

    EXPECT_EQ(workerCtrl.sleepingWorkerNum, -1);
    EXPECT_EQ(workerCtrl.deepSleepingWorkerNum, -1);
    EXPECT_EQ(workerCtrl.executionNum, 1);
}

/**
 * @tc.name: IsExceedDeepSleepThreshold
 * @tc.desc: Test whether the IsExceedDeepSleepThreshold interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, IsExceedDeepSleepThreshold, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    EXPECT_NE(it, nullptr);
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    EXPECT_EQ(cpu.IsExceedDeepSleepThreshold(), false);

    WorkerCtrl& workerCtrl = cpu.ctrlQueue[5];
    workerCtrl.deepSleepingWorkerNum++;
    workerCtrl.executionNum++;
    EXPECT_EQ(cpu.IsExceedDeepSleepThreshold(), true);
}
}
}