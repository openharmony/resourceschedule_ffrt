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


#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include "ffrt_inner.h"
#include "c/loop.h"
#define private public
#include "sync/poller"
#undef private
#include "util.h"

using namespace std;
using namespace ffrt;
using namespace testing;

class PollerTest : public testing::Test {
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

static void Testfun(void* data)
{
    int* testData = static_cast<int*>(data);
    *testData += 1;
    printf("%d, timeout callback\n", *testData);
}
static void (*g_cb)(void*) = Testfun;

HWTEST_F(PollerTest, unregister_timer_001)
{   
    Poller poller;
    // 1、组装timeMap_
    static int result0 = 0;
    int* xf = &result0;
    void* data = xf;
    uint64_t timeout = 1;
    uint64_r sleepTime = 2500;

    // 2、创建两个线程，并发PollerOnce和Unregister
    int para = 1;
    for (int i = 0; i < 50; i++)
    {
        int timerHandle = poller.RegisterTimer(timeout, data, g_cb, true);
        EXPECT_FALSE(poller.timerMap_.empty());
        auto boundPollonce = std::bind(&Poller::PollOnce, &poller, para);
        auto boundUnregister = std::bind(&Poller::UnregisterTimer, &poller, timerHandle);
        usleep(sleepTime);
        std::thread thread1(boundPollonce);
        std::thread thread2(boundUnregister);
        thread1.join();
        thread2.join();
        EXPECT_TRUE(poller.timerMap_.empty());
        EXPECT_TRUE(poller.executedHandle_.empty());
        poller.timerMap_.clear();
        poller.executedHandle_.clear();
    }
}