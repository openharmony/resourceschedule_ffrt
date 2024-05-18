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

#define private public
#define protected public
#include <gtest/gtest.h>
#include <thread>
#include "eu/cpu_worker.h"
#include "eu/scpuworker_manager.h"
#include "eu/scpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "eu/worker_thread.h"
#include "qos.h"
#undef private
#undef protected

using namespace testing;
using namespace testing::ext;
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

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

/**
 * @tc.name: GetMonitorTid
 * @tc.desc: Test whether the GetMonitorTid interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, GetMonitorTid, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    uint32_t tid;
    tid = cpu.GetMonitorTid();
}

/**
 * @tc.name: HandleBlocked
 * @tc.desc: Test whether the HandleBlocked interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, HandleBlocked, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.HandleBlocked(QoS(5));
}

/**
 * @tc.name: DecExeNumRef
 * @tc.desc: Test whether the DecExeNumRef interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, DecExeNumRef, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.DecExeNumRef(QoS(5));
}

/**
 * @tc.name: IncSleepingRef
 * @tc.desc: Test whether the IncSleepingRef interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, IncSleepingRef, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.IncSleepingRef(QoS(5));
}

/**
 * @tc.name: DecSleepingRef
 * @tc.desc: Test whether the DecSleepingRef interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, DecSleepingRef, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.DecSleepingRef(QoS(5));
}

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

    cpu.IntoSleep(QoS(5));
}

/**
 * @tc.name: WakeupCount
 * @tc.desc: Test whether the WakeupCount interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, WakeupCount, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.WakeupCount(QoS(5));
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
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.TimeoutCount(QoS(5));
}

/**
 * @tc.name: RegWorker
 * @tc.desc: Test whether the RegWorker interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, RegWorker, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.RegWorker(QoS(5));
}

/**
 * @tc.name: UnRegWorker
 * @tc.desc: Test whether the UnRegWorker interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, UnRegWorker, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.UnRegWorker();
}

/**
 * @tc.name: Notify
 * @tc.desc: Test whether the Notify interface are normal.
 * @tc.type: FUNC
 *
 *
 */
HWTEST_F(CpuMonitorTest, Notify, TestSize.Level1)
{
    CPUWorkerManager *it = new SCPUWorkerManager();
    SCPUMonitor cpu({
        std::bind(&CPUWorkerManager::IncWorker, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WakeupWorkers, it, std::placeholders::_1),
        std::bind(&CPUWorkerManager::GetTaskCount, it, std::placeholders::_1)});

    cpu.Notify(QoS(5), TaskNotifyType(1));
}
