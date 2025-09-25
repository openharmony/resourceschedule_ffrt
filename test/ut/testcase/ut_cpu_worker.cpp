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
#include "sync/perf_counter.h"
#include "sync/wait_queue.h"

#define private public
#include "eu/cpu_worker.h"
#include "util/white_list.h"
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

namespace {
static inline void stall_us_impl(size_t us)
{
    auto start = std::chrono::system_clock::now();
    size_t passed = 0;
    while (passed < us) {
        passed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now() - start).count();
    }
}

void stall_us(size_t us)
{
    stall_us_impl(us);
}
}

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

static int g_counter = 0;
int SimpleThdFunc(void *)
{
    g_counter++;
    return 0;
}

static void* TempFunc(void *)
{
    ++g_counter;
    return nullptr;
}

WorkerAction WorkerIdleAction(CPUWorker *thread)
{
    return WorkerAction::RETRY;
}
 /* a flag used to stall main thread till the worker is done. This prevents UAF */
std::atomic<bool> workerDone = false;
void WorkerRetired(CPUWorker* thread)
{
    thread->SetExited();
    thread->Detach();
    /* worker is done, set flag to true */
    workerDone = true;
}
void WaitForWorker()
{
    /* Stall till worker is done to prevent UAF */
    while (!workerDone) {
        // spin till the worker is done
    }
}

void WorkerPrepare(CPUWorker* thread)
{
}

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
bool IsBlockAwareInit()
{
    return true;
}
#endif

HWTEST_F(CpuWorkerTest, WorkerStatusTest, TestSize.Level1)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    workerDone = false;
    auto worker = std::make_unique<CPUWorker>(QoS(6), std::move(ops), 1024);
    WorkerStatus status = worker->GetWorkerState();
    EXPECT_EQ(WorkerStatus::EXECUTING, status);
    worker->SetExited();
    WaitForWorker();
}

HWTEST_F(CpuWorkerTest, SetWorkerStatusTest, TestSize.Level1)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    workerDone = false;
    auto worker = std::make_unique<CPUWorker>(QoS(6), std::move(ops), 1024);
    worker->SetWorkerState(WorkerStatus::SLEEPING);
    EXPECT_EQ(WorkerStatus::SLEEPING, worker->state);
    worker->SetExited();
    WaitForWorker();
}

HWTEST_F(CpuWorkerTest, ExitedTest, TestSize.Level1)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    workerDone = false;
    auto worker = std::make_unique<CPUWorker>(QoS(6), std::move(ops), 1024);
    bool ret = worker->Exited();
    EXPECT_FALSE(ret);
    worker->SetExited();
    WaitForWorker();
}

HWTEST_F(CpuWorkerTest, SetExitedTest, TestSize.Level1)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    workerDone = false;
    auto worker = std::make_unique<CPUWorker>(QoS(6), std::move(ops), 1024);
    worker->SetExited();
    EXPECT_TRUE(worker->exited.load());
    WaitForWorker();
}

HWTEST_F(CpuWorkerTest, GetQosTest, TestSize.Level1)
{
    CpuWorkerOps ops;
    ops.WorkerIdleAction = WorkerIdleAction;
    ops.WorkerRetired = WorkerRetired;
    ops.WorkerPrepare = WorkerPrepare;
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    ops.IsBlockAwareInit = IsBlockAwareInit;
#endif
    workerDone = false;
    auto qos = QoS(6);
    auto worker = std::make_unique<CPUWorker>(qos, std::move(ops), 1024);
    EXPECT_EQ(worker->GetQos(), qos);
    worker->SetExited();
    WaitForWorker();
}

/*
 * 测试用例名称：SetCameraThread_Normal_True
 * 测试用例描述：SetThreadAttr进入camera分支
 * 预置条件    ：无
 * 操作步骤    ：1、修改白名单，创建worker时setattr进入camera分支
                2、创建并提交一个任务
 * 预期结果    ：任务执行成功
 */
HWTEST_F(CpuWorkerTest, SetCameraThread_Normal_True, TestSize.Level0)
{
    WhiteList::GetInstance().whiteList_["SetThreadAttr"] = true;
    EXPECT_EQ(WhiteList::GetInstance().IsEnabled("SetThreadAttr", false), true);
    stall_us(5000000);
    std::atomic<int> result = 0;
    for (int i = 0; i < 100; ++i) {
        ffrt::submit([&]{ result.fetch_add(1); });
    }
    ffrt_wait();
    EXPECT_EQ(result, 100);

    WhiteList::GetInstance().whiteList_["SetThreadAttr"] = false;
}