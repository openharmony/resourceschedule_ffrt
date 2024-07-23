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
#undef private
#undef protected

using namespace ffrt;
using namespace testing::ext;

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
