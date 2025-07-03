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
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <sched.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include "c/thread.h"
#include "eu/parse_ini.h"
#include "sync/perf_counter.h"
#include "sync/wait_queue.h"

#define private public
#include "eu/cpu_worker.h"
#undef private
#include "../common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;
#ifdef APP_USE_ARM
static const size_t WORKER_STACK_SIZE = 131072;
#else
static const size_t WORKER_STACK_SIZE = 10 * 1024 * 1024;
#endif

class CpuWorkerTest : public testing::Test {
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

    static void* MockStart(void* arg)
    {
        int* inc = reinterpret_cast<int*>(arg);
        (*inc) += 1;
        return nullptr;
    }
};

static int conunter = 0;
int simple_thd_func(void *)
{
    counter++;
    return 0;
}

static void* tmp_func(void *)
{
    ++counter;
    return nullptr;
}

WorkerAction WorkerIdleAction(CPUWorker *thread)
{
    return WorkerAction::RETRY;
}

void WorkerRetired(CPUWorker* thread)
{
}

void WorkerPrepare(CPUWorker* thread)
{
}

PollerRet TryPoll(const CPUWorker*, int timeout)
{
    return PollerRet::RET_NULL;
}

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
bool IsBlockAwareInit()
{
    return true;
}
#endif

HWTEST_F(CpuWorkerTest, WorkerStatusTest, TestSize.Level0)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
    ops.TryPoll = TryPoll;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    CPUWorker *worker = new CPUWorker(QoS(6), std::move(ops), 1024);
    WorkerStatus status = worker->GetWorkerState();
    EXPECT_EQ(WorkerStatus::EXECUTING, status);
}

HWTEST_F(CpuWorkerTest, SetWorkerStatusTest, TestSize.Level0)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
    ops.TryPoll = TryPoll;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    CPUWorker *worker = new CPUWorker(QoS(6), std::move(ops), 1024);
    worker->SetWorkerState(WorkerStatus::SLEEPING);
    EXPECT_EQ(WorkerStatus::SLEEPING, worker->state);
}

HWTEST_F(CpuWorkerTest, ExitedTest, TestSize.Level0)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
    ops.TryPoll = TryPoll;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    CPUWorker *worker = new CPUWorker(QoS(6), std::move(ops), 1024);
    bool ret = worker->Exited();
    EXPECT_FALSE(ret);
}

HWTEST_F(CpuWorkerTest, SetExitedTest, TestSize.Level0)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
    ops.TryPoll = TryPoll;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    CPUWorker *worker = new CPUWorker(QoS(6), std::move(ops), 1024);
    bool var = false;
    worker->SetExited(var);
    EXPECT_FALSE(worker->exited);
}

HWTEST_F(CpuWorkerTest, GetQosTest, TestSize.Level0)
{
    CpuWorkerQos ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
    ops.TryPoll = TryPoll;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    CPUWorker *worker = new CPUWorker(QoS(6), std::move(ops), 1024);
    EXPECT_EQ(worker->GetQos(), 6);
}