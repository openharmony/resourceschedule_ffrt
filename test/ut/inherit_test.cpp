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
#include "ffrt_inner.h"
#include "c/executor_task.h"
#include "tm/scpu_task.h"
#include "dfx/log/ffrt_log_api.h"

using namespace std;
using namespace ffrt;
using namespace testing;
using namespace testing::ext;

class InheritTest : public testing::Test {
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

HWTEST_F(InheritTest, executorToCPUEUTask, TestSize.Level1)
{
    ffrt_executor_task_t w;
    w.type = 1;
    auto* cputask = reinterpret_cast<SCPUEUTask*>(&w);
    printf("w.addr:%llx\n", reinterpret_cast<uint64_t>(&w));
    printf("w.type Addr%llx\n", reinterpret_cast<uint64_t>(&(w.type)));

    printf("cputask base Addr %llx\n", reinterpret_cast<uint64_t>(cputask));
    printf("cputask.type TaskBase Addr%llx\n", reinterpret_cast<uint64_t>(&(cputask->type)));
    printf("cputask.rc Taskdeleter Addr %llx\n", reinterpret_cast<uint64_t>(&cputask->rc));
    printf("cputask.wue Addr %llx\n", reinterpret_cast<uint64_t>(&cputask->wue));

    EXPECT_EQ(w.type, 1);
    EXPECT_EQ(cputask->type, 1);
    EXPECT_EQ(reinterpret_cast<uint64_t>(&(w.type)), reinterpret_cast<uint64_t>(&(cputask->type)));
}