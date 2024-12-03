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
#include "c/thread.h"
#include "sync/perf_counter.h"
#include "sync/wait_queue.h"

#define private public
#include "eu/worker_thread.h"
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

class ThreadTest : public testing::Test {
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

    static void* MockStart(void* arg)
    {
        int* inc = reinterpret_cast<int*>(arg);
        (*inc) += 1;
        return nullptr;
    }
};

static int counter = 0;
int simple_thd_func(void *)
{
    counter++;
    return 0;
}

void* MyFunc(void * arg)
{
    int *cnter = (int *)arg;
    (*cnter)++;
    return arg;
}

HWTEST_F(ThreadTest, IdleTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool ret = wt->Idle();
    EXPECT_FALSE(ret);
}

HWTEST_F(ThreadTest, SetIdleTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool var = false;
    wt->SetIdle(var);
    EXPECT_FALSE(wt->idle);
}

HWTEST_F(ThreadTest, ExitedTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool ret = wt->Exited();
    EXPECT_FALSE(ret);
}

HWTEST_F(ThreadTest, SetExitedTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    bool var = false;
    wt->SetExited(var);
    EXPECT_FALSE(wt->exited);
}

HWTEST_F(ThreadTest, GetQosTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    EXPECT_NE(wt, nullptr);
    QoS ret = wt->GetQos();
}

HWTEST_F(ThreadTest, JoinTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    EXPECT_NE(wt, nullptr);
    wt->Join();
}

HWTEST_F(ThreadTest, DetachTest, TestSize.Level1)
{
    WorkerThread* wt = new WorkerThread(QoS(6));
    EXPECT_NE(wt, nullptr);
    wt->Detach();
}

HWTEST_F(ThreadTest, set_worker_stack_size, TestSize.Level1)
{
    int inc = 0;
    size_t stackSize = 0;
    WorkerThread* wt = new WorkerThread(QoS(6));
    wt->NativeConfig();
    wt->Start(MockStart, &inc);
    wt->Join();
    EXPECT_EQ(inc, 1);
    delete wt;

    ffrt_error_t ret = ffrt_set_worker_stack_size(5, 10);
    EXPECT_EQ(ret, ffrt_error_inval);

    ret = ffrt_set_worker_stack_size(5, WORKER_STACK_SIZE);
    wt = new WorkerThread(QoS(5));
    wt->NativeConfig();
    wt->Start(MockStart, &inc);
    wt->Join();
    EXPECT_EQ(inc, 2);
    pthread_attr_getstacksize(&wt->attr_, &stackSize);
    EXPECT_EQ(stackSize, WORKER_STACK_SIZE);
    delete wt;
}

HWTEST_F(ThreadTest, c_api_thread_simple_test, TestSize.Level1)
{
    ffrt_thread_t thread;
    ffrt_thread_create(&thread, nullptr, nullptr, nullptr);
    ffrt_thread_detach(nullptr);
    ffrt_thread_join(nullptr, nullptr);
}

HWTEST_F(ThreadTest, c_api_thread_simple_test2, TestSize.Level1)
{
    ffrt_thread_t thread;
    ffrt_thread_attr_t attr;
    attr.storage[0] = 12345;
    int a = 0;
    ffrt_thread_create(&thread, &attr, MyFunc, &a);
    ffrt_thread_detach(&thread);
    ffrt_thread_join(nullptr, nullptr);
}

HWTEST_F(ThreadTest, wait_queue_test, TestSize.Level1)
{
    ffrt::submit([]{
        TaskWithNode node = TaskWithNode();
        EXPECT_NE(node.task, nullptr);
    }, {}, {});
}
