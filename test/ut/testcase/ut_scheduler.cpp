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

#include <list>
#include <vector>
#include <queue>
#include <thread>
#include <gtest/gtest.h>
#define private public
#define protected public
#include "ffrt_inner.h"

#include "core/entity.h"
#include "sched/task_scheduler.h"
#include "sched/scheduler.h"
#include "core/task_attr_private.h"
#include "tm/scpu_task.h"
#include "tm/task_base.h"
#include "tm/io_task.h"
#include "sched/stask_scheduler.h"
#include "util/ffrt_facade.h"
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class SchedulerTest : public testing::Test {
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

HWTEST_F(SchedulerTest, ffrt_task_runqueue_test, TestSize.Level0)
{
    auto fifoqueue = std::make_unique<ffrt::FIFOQueue>();
    constexpr int enqCount = 10;
    SCPUEUTask task(nullptr, nullptr, 0);
    for (int i = 0; i < enqCount ; i++) {
        fifoqueue->EnQueue(&task);
    }
    EXPECT_EQ(fifoqueue->Size(), enqCount);
    EXPECT_EQ(fifoqueue->Empty(), false);
}