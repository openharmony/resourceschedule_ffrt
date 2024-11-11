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
#include "sync/poller.h"
#undef private
#include "util.h"
#include "../common.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
 
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
 
/*
 * 测试用例名称：poll_once_batch_timeout
 * 测试用例描述：PollOnce批量超时测试
 * 预置条件    ：无
 * 操作步骤    ：1、调用注册接口
                2、单次调用PollOnce
 * 预期结果    ：1、超时任务全部从队列中清除
                2、当实际超时时间很长，但是传入超时时间很短，应该按传入时间执行
 */
HWTEST_F(PollerTest, poll_once_batch_timeout, TestSize.Level1)
{
    Poller poller;
    // 1、组装timeMap_
    static int result0 = 0;
    int* xf = &result0;
    void* data = xf;
    uint64_t timeout = 10;
    uint64_t timeout1 = 11;
    uint64_t timeout2 = 12;
    uint64_t sleepTime = 25000;
    poller.RegisterTimer(timeout, data, g_cb, false);
    poller.RegisterTimer(timeout1, data, g_cb, false);
    poller.RegisterTimer(timeout2, data, g_cb, false);
    // 调用PollOnce,预计timerMap_为空，全部清除
    usleep(sleepTime);
    poller.PollOnce(1);
    EXPECT_EQ(true, poller.DetermineEmptyMap());
 
    uint64_t timeout3 = 10000;
    uint64_t timeout4 = 100;
    int loopNum = 2;
    poller.RegisterTimer(timeout3, data, g_cb, false);
    TimePoint start = std::chrono::steady_clock::now();
    for (int i = 0; i < loopNum; ++i) {
        poller.PollOnce(timeout4);
    }
    TimePoint end = std::chrono::steady_clock::now();
    int m = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // 预计等待时间为100，可能有几毫秒的误差
    EXPECT_EQ(true, m >= timeout4 && m < timeout3);
}
 
/*
 * 测试用例名称：cache_events_mask_test
 * 测试用例描述：本地events缓存
 * 预置条件    ：无
 * 操作步骤    ：新增event缓存
 * 预期结果    ：task对应的events缓存内有没有新增
 */
HWTEST_F(PollerTest, cache_events_mask_test, TestSize.Level1)
{
    Poller poller;
    CPUEUTask* currTask;
    EventVec eventVec;
    epoll_event ev;
    eventVec.push_back(ev);
    poller.CacheEventsAndDoMask(currTask, eventVec);
    EXPECT_EQ(1, poller.m_cachedTaskEvents[currTask].size());
}
 
/*
 * 测试用例名称：fetch_cached_event_unmask
 * 测试用例描述：遍历本地events缓存，并提出缓存event
 * 预置条件    ：无
 * 操作步骤    ：1、新增event缓存 2、清除缓存
 * 预期结果    ：task对应的events看有没有去除
 */
HWTEST_F(PollerTest, fetch_cached_event_unmask, TestSize.Level1)
{
    Poller poller;
    CPUEUTask* currTask;
    EventVec eventVec;
    epoll_event ev;
    struct epoll_event events[1024];
    eventVec.push_back(ev);
    poller.CacheEventsAndDoMask(currTask, eventVec);
    int fdCnt = poller.FetchCachedEventAndDoUnmask(currTask, events);
    EXPECT_EQ(1, fdCnt);
}
/*
 * 测试用例名称：poll_once_batch_timeout
 * 测试用例描述：PollOnce批量超时测试
 * 预置条件    ：无
 * 操作步骤    ：1、调用注册接口,注册repeat为true的timer
                2、创建两个线程，并发PollerOnce和Unregister
 * 预期结果    ：1、任务全部从队列中清除
 */
HWTEST_F(PollerTest, unregister_timer_001, TestSize.Level1)
{
    Poller poller;
    // 1、组装timeMap_
    static int result0 = 0;
    int* xf = &result0;
    void* data = xf;
    uint64_t timeout = 1;
    uint64_t sleepTime = 2500;
    int maxIter = 100;
    // 2、 创建两个线程，并发PollerOnce和Unregister
    for (int i = 0; i < maxIter; i++) {
        int timerHandle = poller.RegisterTimer(timeout, data, g_cb, true);
        EXPECT_FALSE(poller.timerMap_.empty());
        auto boundPollonce = std::bind(&Poller::PollOnce, &poller, timeout);
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