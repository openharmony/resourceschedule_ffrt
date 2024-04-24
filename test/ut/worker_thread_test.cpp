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
#include <cstring>
#include <algorithm>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/resource.h>

#define private public
#include "eu/worker_thread.h"
#undef private

using namespace testing;
using namespace testing::ext;
using namespace ffrt;

class WorkerThreadTest : public testing::Test {
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
 * @tc.name: IdleTest
 * @tc.desc: Test whether the Idle interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerThreadTest, IdleTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool ret = wt->Idle();
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: SetIdleTest
 * @tc.desc: Test whether the SetIdle interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerThreadTest, SetIdleTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool var = false;
    wt->SetIdle(var);
    EXPECT_FALSE(wt->idle);
}

/**
 * @tc.name: ExitedTest
 * @tc.desc: Test whether the Exited interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerThreadTest, ExitedTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool ret = wt->Exited();
    EXPECT_FALSE(ret);
}

/**
 * @tc.name: SetExitedTest
 * @tc.desc: Test whether the SetExited interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerThreadTest, SetExitedTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool var = false;
    wt->SetExited(var);
    EXPECT_FALSE(wt->exited);
}

/**
 * @tc.name: GetQosTest
 * @tc.desc: Test whether the GetQos interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerThreadTest, GetQosTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    QoS ret = wt->GetQos();
}

/**
 * @tc.name: JoinTest
 * @tc.desc: Test whether the Join interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerThreadTest, JoinTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    wt->Join();
}

/**
 * @tc.name: DetachTest
 * @tc.desc: Test whether the Detach interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(WorkerThreadTest, DetachTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    wt->Detach();
}
