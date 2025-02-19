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
#ifndef WITH_NO_MOCKER
#include <mockcpp/mockcpp.hpp>
#endif
#include <thread>
#include <climits>
#include <cstring>
#define private public
#define protected public
#include "eu/worker_manager.h"
#include "eu/scpuworker_manager.h"
#include "sched/task_scheduler.h"
#include "tm/scpu_task.h"
#include "sched/scheduler.h"
#undef private
#undef protected
#include "../common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class WorkerManagerTest : public testing::Test {
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

HWTEST_F(WorkerManagerTest, IncWorkerTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS(-1);
    bool iw = cm->IncWorker(*qos);
    EXPECT_FALSE(iw);

    delete qos;
    delete cm;
}

HWTEST_F(WorkerManagerTest, IncWorkerTest2, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS(-1);
    cm->tearDown = true;
    bool iw2 = cm->IncWorker(*qos);
    EXPECT_FALSE(iw2);

    delete qos;
    delete cm;
}

HWTEST_F(WorkerManagerTest, GetWorkerCountTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS(2);
    cm->GetWorkerCount(*qos);
    EXPECT_EQ(cm->GetWorkerCount(*qos), 0);

    delete qos;
    delete cm;
}

HWTEST_F(WorkerManagerTest, JoinTGTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS(ffrt::qos_deadline_request);
    ThreadGroup* tg = cm->JoinTG(*qos);
    EXPECT_NE(tg, nullptr);

    QoS* qos1 = new QoS(ffrt::qos_user_interactive);
    ThreadGroup* tg1 = cm->JoinTG(*qos1);
    EXPECT_EQ(tg1, nullptr);
    delete cm;
    delete qos;
    delete qos1;
}

HWTEST_F(WorkerManagerTest, LeaveTGTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS(ffrt::qos_deadline_request);
    bool ltg = cm->IncWorker(*qos);
    EXPECT_TRUE(ltg);
#ifndef WITH_NO_MOCKER
    MOCKER_CPP(&RTGCtrl::GetThreadGroup).stubs().will(returnValue(1));
    MOCKER_CPP(&RTGCtrl::PutThreadGroup).stubs().will(returnValue(true));
    MOCKER_CPP(&RTGCtrl::JoinThread).stubs().will(returnValue(true));
    MOCKER_CPP(&RTGCtrl::RemoveThread).stubs().will(returnValue(true));
#endif
    ThreadGroup* ltg1 = cm->JoinTG(*qos);
    EXPECT_NE(ltg1, nullptr);

    cm->LeaveTG(*qos);

    delete qos;
    delete cm;
#ifndef WITH_NO_MOCKER
    GlobalMockObject::verify();
#endif
}

int GetTaskCountStub(const QoS& qos)
{
    return 1;
}

/*
 * 测试用例名称：CPUMonitorHandleTaskNotifyUltraConservativeTest
 * 测试用例描述：ffrt保守调度策略
 * 预置条件    ：创建SCPUWorkerManager，策略设置为HandleTaskNotifyUltraConservative，GetTaskCount方法打桩为GetTaskCountStub
 * 操作步骤    ：调用SCPUWorkerManager的Notify方法
 * 预期结果    ：成功执行HandleTaskNotifyUltraConservative方法
 */
HWTEST_F(WorkerManagerTest, CPUMonitorHandleTaskNotifyUltraConservativeTest, TestSize.Level1)
{
    SCPUWorkerManager* manager = new SCPUWorkerManager();

    CpuMonitorOps monitorOps { // change monitor's notify handle strategy
        std::bind(&SCPUWorkerManager::IncWorker, manager, std::placeholders::_1),
        [=] (const QoS& qos) {
            WorkerCtrl& workerCtrl = manager->monitor->ctrlQueue[2];
            workerCtrl.sleepingWorkerNum--;
        },
        std::bind(&GetTaskCountStub, std::placeholders::_1),
        std::bind(&SCPUWorkerManager::GetWorkerCount, manager, std::placeholders::_1),
        CPUMonitor::HandleTaskNotifyUltraConservative,
    };
    manager->monitor->ops = std::move(monitorOps);
    WorkerCtrl& workerCtrl = manager->monitor->ctrlQueue[2];
    EXPECT_EQ(workerCtrl.executionNum, 0);

    manager->NotifyTaskAdded(QoS(2)); // task notify event
    EXPECT_EQ(workerCtrl.executionNum, 1);

    manager->monitor->ctrlQueue[2].sleepingWorkerNum = 1;
    manager->NotifyTaskAdded(QoS(2)); // task notify event
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 0);
    delete manager;
}

HWTEST_F(WorkerManagerTest, CPUMonitorHandleTaskNotifyConservativeTest, TestSize.Level1)
{
    SCPUWorkerManager* manager = new SCPUWorkerManager();

    CpuMonitorOps monitorOps { // change monitor's notify handle strategy
        std::bind(&SCPUWorkerManager::IncWorker, manager, std::placeholders::_1),
        [=] (const QoS& qos) {
            WorkerCtrl& workerCtrl = manager->monitor->ctrlQueue[2];
            workerCtrl.sleepingWorkerNum--;
        },
        std::bind(&GetTaskCountStub, std::placeholders::_1),
        std::bind(&SCPUWorkerManager::GetWorkerCount, manager, std::placeholders::_1),
        CPUMonitor::HandleTaskNotifyConservative,
    };
    manager->monitor->ops = std::move(monitorOps);
    WorkerCtrl& workerCtrl = manager->monitor->ctrlQueue[2];
    EXPECT_EQ(workerCtrl.executionNum, 0);

    manager->NotifyTaskAdded(QoS(2)); // task notify event
    EXPECT_EQ(workerCtrl.executionNum, 1);

    manager->monitor->ctrlQueue[2].sleepingWorkerNum = 1;
    manager->NotifyTaskAdded(QoS(2)); // task notify event
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 0);
}

HWTEST_F(WorkerManagerTest, CPUMonitorHandleTaskNotifyConservativeTest2, TestSize.Level1)
{
    SCPUWorkerManager* manager = new SCPUWorkerManager();
    TaskNotifyType::TASK_PICKED;

    CpuMonitorOps monitorOps { // change monitor's notify handle strategy
        std::bind(&SCPUWorkerManager::IncWorker, manager, std::placeholders::_1),
        [=] (const QoS& qos) {
            WorkerCtrl& workerCtrl = manager->monitor->ctrlQueue[2];
            workerCtrl.sleepingWorkerNum--;
        },
        std::bind(&GetTaskCountStub, std::placeholders::_1),
        std::bind(&SCPUWorkerManager::GetWorkerCount, manager, std::placeholders::_1),
        std::bind(&CPUMonitor::HandleTaskNotifyConservative,
            std::placeholders::_1, std::placeholders::_2, TaskNotifyType::TASK_PICKED),
    };
    manager->monitor->ops = std::move(monitorOps);
    WorkerCtrl& workerCtrl = manager->monitor->ctrlQueue[2];
    EXPECT_EQ(workerCtrl.executionNum, 0);

    manager->NotifyTaskAdded(QoS(2)); // task notify event
    EXPECT_EQ(workerCtrl.executionNum, 1);

    manager->monitor->ctrlQueue[2].sleepingWorkerNum = 1;
    manager->NotifyTaskAdded(QoS(2)); // task notify event
    EXPECT_EQ(workerCtrl.sleepingWorkerNum, 1);
}

HWTEST_F(WorkerManagerTest, PickUpTaskBatch, TestSize.Level1)
{
    CPUWorkerManager* manager = new SCPUWorkerManager();
    CPUManagerStrategy* strategy = new CPUManagerStrategy();
    SCPUEUTask* task1 = new SCPUEUTask(nullptr, nullptr, 0, QoS(qos(0)));
    SCPUEUTask* task2 = new SCPUEUTask(nullptr, nullptr, 0, QoS(qos(0)));
    CPUMonitor* monitor = manager->GetCPUMonitor();

    auto worker1 = strategy->CreateCPUWorker(QoS(qos(0)), manager);
    auto worker2 = strategy->CreateCPUWorker(QoS(qos(0)), manager);

    monitor->WakeupDeepSleep(QoS(qos(0)), false);
    monitor->WakeupDeepSleep(QoS(qos(0)), false);

    auto& sched1 = FFRTScheduler::Instance()->GetScheduler(worker1->GetQos());
    auto& sched2 = FFRTScheduler::Instance()->GetScheduler(worker2->GetQos());

    EXPECT_EQ(sched1.WakeupTask(reinterpret_cast<CPUEUTask*>(task1)), 1);
    EXPECT_EQ(sched2.WakeupTask(reinterpret_cast<CPUEUTask*>(task2)), 1);

    EXPECT_NE(manager->PickUpTaskBatch(worker1), nullptr);
    EXPECT_NE(manager->PickUpTaskBatch(worker2), nullptr);

    delete manager;
    delete worker1;
    delete worker2;
    delete task1;
    delete task2;
    delete strategy;
}