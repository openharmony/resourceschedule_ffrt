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
#include "eu/cpu_monitor.h"
#include "eu/cpu_worker.h"
#include "eu/scpuworker_manager.h"
#include "eu/cpu_manager_interface.h"
#include "eu/worker_thread.h"
#include "qos.h"

using namespace testing;
using namespace testing::ext;
using namespace ffrt;
using namespace std;


class CpuworkerManagerTest : public testing::Test {
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
 * @tc.name: NotifyTaskAdded
 * @tc.desc: Test whether the NotifyTaskAdded interface are normal.
 * @tc.type: FUNC
 */
HWTEST_F(CpuworkerManagerTest, NotifyTaskAdded, TestSize.Level1)
{
    auto *it = new SCPUWorkerManager();
    it->NotifyTaskAdded(QoS(qos(5)));
}
