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
#include <climits>
#include <cstring>
#include "eu/worker_manager.h"
#include "eu/scpuworker_manager.h"
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "common.h"

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

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

/**
 * @tc.name: JoinRtgTest
 * @tc.desc: Test whether the JoinRtg interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerManagerTest, JoinRtgTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS();
    cm->JoinRtg(*qos);
}

/**
 * @tc.name: JoinTGTest
 * @tc.desc: Test whether the JoinTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerManagerTest, JoinTGTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS(ffrt_qos_deadline_request);
    ThreadGroup* tg = cm->JoinTG(*qos);
    EXPECT_NE(tg, nullptr);

    QoS* qos1 = new QoS(ffrt_qos_user_interactive);
    ThreadGroup* tg1 = cm->JoinTG(*qos1);
    EXPECT_EQ(tg1, nullptr);
}

/**
 * @tc.name: LeaveTGTest
 * @tc.desc: Test whether the LeaveTG interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerManagerTest, LeaveTGTest, TestSize.Level1)
{
    CPUWorkerManager* cm = new SCPUWorkerManager();
    QoS* qos = new QoS();
    cm->LeaveTG(*qos);
}
