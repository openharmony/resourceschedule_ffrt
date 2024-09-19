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

using namespace ffrt;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class WorkerManagerTest : public testing::Test {
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

HWTEST_F(WorkerManagerTest, JoinRtgTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS();
    cm->IncWorker(*qos);
    cm->JoinRtg(*qos);

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
}

HWTEST_F(WorkerManagerTest, LeaveTGTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS(ffrt::qos_deadline_request);
    cm->IncWorker(*qos);
#ifndef WITH_NO_MOCKER
    MOCKER_CPP(&RTGCtrl::GetThreadGroup).stubs().will(returnValue(1));
    MOCKER_CPP(&RTGCtrl::PutThreadGroup).stubs().will(returnValue(true));
    MOCKER_CPP(&RTGCtrl::JoinThread).stubs().will(returnValue(true));
    MOCKER_CPP(&RTGCtrl::RemoveThread).stubs().will(returnValue(true));
#endif
    cm->JoinTG(*qos);
    cm->LeaveTG(*qos);

    delete qos;
    delete cm;
#ifndef WITH_NO_MOCKER
    GlobalMockObject::verify();
#endif
}

HWTEST_F(WorkerManagerTest, CPUManagerStrategyApiTest, TestSize.Level1)
{
    WorkerManager* manager = new SCPUWorkerManager();

    CPUMonitor* monitor = CPUManagerStrategy::CreateCPUMonitor(QoS(2), manager);
    EXPECT_NE(monitor, nullptr);
    delete monitor;

    WorkerThread* worker = CPUManagerStrategy::CreateCPUWorker(QoS(2), manager);
    EXPECT_NE(worker, nullptr);

    delete manager;
    worker->Join();
    delete worker;
}

HWTEST_F(WorkerManagerTest, CPUWorkerStandardLoopTest, TestSize.Level1)
{
    CPUWorkerManager* manager = new SCPUWorkerManager();

    CpuWorkerOps ops {
        CPUWorker::WorkerLooperStandard,
        std::bind(&CPUWorkerManager::PickUpTaskFromGlobalQueue, manager, std::placeholders::_1),
        std::bind(&CPUWorkerManager::NotifyTaskPicked, manager, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerIdleActionSimplified, manager, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerRetired, manager, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerPrepare, manager, std::placeholders::_1),
    };
    WorkerThread* worker = new CPUWorker(QoS(2), std::move(ops));
    EXPECT_NE(worker, nullptr);
    sleep(1);
    manager->NotifyTaskAdded(QoS(2));

    delete manager;
    worker->Join();
    delete worker;
}

TEST_F(WorkerManagerTest, PickUpTaskFromGlobalQueue)
{
    CPUWorkerManager* manager = new SCPUWorkerManager();
    CPUManagerStrategy* strategy = new CPUManagerStrategy();
    SCPUEUTask* task = new SCPUEUTask(nullptr, nullptr, 0, QoS(qos(0)));

    auto worker = strategy->CreateCPUWorker(QoS(qos(0)), manager);
    auto& sched = FFRTScheduler::Instance()->GetScheduler(worker->GetQos());

    int ret = sched.WakeupTask(reinterpret_cast<CPUEUTask*>(task));
    EXPERT_EQ(ret, 1);

    auto pickTask = manager->PickUpTaskFromGlobalQueue(worker);
    EXPECT_NE(pickTask, nullptr);

    delete worker;
    delete task;
    delete strategy;
    delete manager;
}

TEST_F(WorkerManagerTest, PickUpTaskBatch)
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

    delete worker1;
    delete worker2;
    delete task1;
    delete task2;
    delete strategy;
    delete manager;
}