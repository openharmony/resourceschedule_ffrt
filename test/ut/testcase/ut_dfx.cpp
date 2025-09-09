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
#include <chrono>
#include "ffrt_inner.h"
#include "dfx/bbox/bbox.h"
#define private public
#include "util/worker_monitor.h"
#include "eu/loop_poller.h"
#undef private
#include "c/queue_ext.h"
#include "../common.h"
#ifdef FFRT_ENABLE_HITRACE_CHAIN
#include "dfx/trace/ffrt_trace_chain.h"
#endif
using namespace ffrt;

extern void SaveTheBbox();
extern void RecordDebugInfo();

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class DfxTest : public testing::Test {
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

HWTEST_F(DfxTest, tracetest, TestSize.Level0)
{
    int x = 0;
    ffrt::submit(
        [&]() {
            ffrt::set_trace_tag("task");
            x++;
            ffrt::clear_trace_tag();
        }, {}, {});
    ffrt::wait();
    EXPECT_EQ(x, 1);
}

static struct sigaction s_oldSa[SIGSYS + 1]; // SIGSYS = 31

static void SignalHandler(int signo, siginfo_t* info, void* context __attribute__((unused)))
{
    SaveTheBbox();

    // we need to deregister our signal handler for that signal before continuing.
    sigaction(signo, &s_oldSa[signo], nullptr);
}

static void SignalReg(int signo)
{
    sigaction(signo, nullptr, &s_oldSa[signo]);
    struct sigaction newAction;
    newAction.sa_flags = SA_RESTART | SA_SIGINFO;
    newAction.sa_sigaction = SignalHandler;
    sigaction(signo, &newAction, nullptr);
}

HWTEST_F(DfxTest, queue_dfx_bbox_normal_task, TestSize.Level0)
{
    // 异常信号用例，测试bbox功能正常；
    int x = 0;
    ffrt::mutex lock;

    pid_t pid = fork();
    if (!pid) {
        printf("pid = %d, thread id= %d  start\n", getpid(), pid);

        auto basic1Func = [&]() {
            std::lock_guard lg(lock);
            ffrt_usleep(2000);
            x = x + 1;

        };

        auto basic2Func = [&]() {
            ffrt_usleep(3000);
            SignalReg(SIGABRT);
            raise(SIGABRT);  // 向自身进程发送SIGABR
        };

        auto basic3Func = [&]() {
            ffrt_usleep(5000);
            std::lock_guard lg(lock);
            x = x + 1;
        };

        for (int i = 0; i < 20; i++) {
            ffrt::submit(basic1Func, {}, {}, ffrt::task_attr().qos(static_cast<int>(ffrt::qos_default)));
        }
        for (int i = 0; i < 10; i++) {
            ffrt::submit(basic3Func, {}, {}, ffrt::task_attr().qos(static_cast<int>(ffrt::qos_default)));
        }
        auto task = ffrt::submit_h(basic2Func, {}, {}, ffrt::task_attr().qos(static_cast<int>(ffrt::qos_background)));

        ffrt::wait({task});
        printf("pid = %d, thread id= %d  end\n", getpid(), pid);
        exit(0);
    }
    sleep(1);
}

HWTEST_F(DfxTest, queue_dfx_bbox_queue_task, TestSize.Level0)
{
    // 异常信号用例，测试bbox功能正常；
    int x = 0;
    ffrt::mutex lock;

    pid_t pid = fork();
    if (!pid) {
        printf("pid = %d, thread id= %d  start\n", getpid(), pid);
        ffrt_queue_attr_t queue_attr;
        ffrt_queue_attr_t queue_attr2;
        (void)ffrt_queue_attr_init(&queue_attr); // 初始化属性，必须
        ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);
        (void)ffrt_queue_attr_init(&queue_attr2); // 初始化属性，必须
        ffrt_queue_t queue_handle2 = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr2);
        std::function<void()> basic1Func = [&]() {
            lock.lock();
            ffrt_usleep(5000);
            x = x + 1;
            lock.unlock();
        };

        auto basic3Func = [&]() {
            lock.lock();
            sleep(2);
            lock.unlock();
        };

        auto task = ffrt::submit_h(basic3Func, {}, {}, ffrt::task_attr().qos(static_cast<int>(ffrt::qos_background)));
        ffrt_queue_submit(queue_handle, create_function_wrapper(basic1Func, ffrt_function_kind_queue), nullptr);
        ffrt_queue_submit(queue_handle2, create_function_wrapper(basic1Func, ffrt_function_kind_queue), nullptr);

        SaveTheBbox();
        ffrt::wait({task});
        ffrt_queue_attr_destroy(&queue_attr);
        ffrt_queue_destroy(queue_handle);
        ffrt_queue_attr_destroy(&queue_attr2);
        ffrt_queue_destroy(queue_handle2);
        printf("pid = %d, thread id= %d  end\n", getpid(), pid);
        exit(0);
    }
    sleep(1);
}

/*
 * 测试用例名称：dfx_bbox_save_record
 * 测试用例描述：提交有依赖关系的任务，测试SaveTheBbox和RecordDebugInfo函数
 * 预置条件    ：无
 * 操作步骤    ：提交嵌套任务，子任务等待cv和数据依赖，调用SaveTheBbox和RecordDebugInfo接口
 * 预期结果    ：SaveTheBbox()函数正确执行
 */
HWTEST_F(DfxTest, dfx_bbox_save_record, TestSize.Level0)
{
    int x = 0;
    SaveTheBbox();
    ffrt::submit([]() {
        RecordDebugInfo();
        x++;
    });
    ffrt::wait();
    EXPECT_EQ(x, 1);
}

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

/*
 * 测试用例名称：normaltsk_timeout_executing
 * 测试用例描述：并发任务WorkerMonitor检测到任务执行超时
 * 操作步骤    ：1、提交执行时间长任务
 * 预期结果    ：成功触发任务执行超时告警
 */
HWTEST_F(DfxTest, normaltsk_timeout_executing, TestSize.Level1)
{
    ffrt::WorkerMonitor::GetInstance().timeoutUs_ = 1000000;
    ffrt::WorkerMonitor::GetInstance().SubmitTaskMonitor(1000000);
    ffrt_task_timeout_set_threshold(1000);
    int x = 0;
    auto h = ffrt::submit_h(
        [&]() {
            stall_us(3000000);
            x++;
        }, {}, {});
    ffrt::wait({h});
    EXPECT_EQ(x, 1);
}

/*
 * 测试用例名称：normaltsk_timeout_pending
 * 测试用例描述：并发任务WorkerMonitor检测到任务调度超时
 * 操作步骤    ：1、限制worker数为1
               2、提交1个执行时间长任务占住worker，1个即时任务等待
 * 预期结果    ：触发PENDING超时告警
 */
HWTEST_F(DfxTest, normaltsk_timeout_pending, TestSize.Level1)
{
    ffrt_task_timeout_set_threshold(1000);
    ffrt_set_cpu_worker_max_num(qos_default, 2);
    constexpr int numTasks = 3;
    std::atomic<int> x = 0;

    for (int i = 0; i < numTasks; i++) {
        ffrt::submit(
            [&]() {
                stall_us(2000000);
                x.fetch_add(1, std::memory_order_relaxed);
            }, {}, {});
    }
    ffrt::wait();
    EXPECT_EQ(x, numTasks);
}

/*
 * 测试用例名称：normaltsk_timeout_multi
 * 测试用例描述：多个并发任务WorkerMonitor检测到超时
 * 操作步骤    ：1、提交多个执行时间长任务
 * 预期结果    ：正确触发多次超时告警
 */
HWTEST_F(DfxTest, normaltsk_timeout_multi, TestSize.Level1)
{
    ffrt_task_timeout_set_threshold(1000);
    constexpr int numTasks = 5;
    std::atomic<int> x = 0;

    for (int i = 0; i < numTasks; i++) {
        ffrt::submit(
            [&]() {
                stall_us(1500000);
                x.fetch_add(1, std::memory_order_relaxed);
            }, {}, {});
    }
    ffrt::wait();
    EXPECT_EQ(x, numTasks);
}

/*
 * 测试用例名称：normaltsk_timeout_delay
 * 测试用例描述：并发任务WorkerMonitor不检测延时任务的主动延时
 * 操作步骤    ：1、提交延时任务
 * 预期结果    ：主动延时期间不触发超时告警，但任务执行期间触发
 */
HWTEST_F(DfxTest, normaltsk_timeout_delay, TestSize.Level1)
{
    ffrt_task_timeout_set_threshold(1000);
    constexpr int numTasks = 2;
    std::atomic<int> x = 0;
    for (int i = 0; i < numTasks; i++) {
        ffrt::submit([&]() {
            std::cout << "delay " << 1500 << " us\n";
            x.fetch_add(1, std::memory_order_relaxed);
            stall_us(1500000);
            }, {}, {}, ffrt::task_attr().delay(1500000));
    }
    ffrt::wait();
    EXPECT_EQ(x, numTasks);
    ffrt::WorkerMonitor::GetInstance().timeoutUs_ = 30000000;
}

void MyCallback1(uint64_t id, const char* message, uint32_t length)
{
    FFRT_LOGE("call ffrt_queue_monitor timeout_callback");
}

static void Testfun(void* data)
{
    int* testData = static_cast<int*>(data);
    *testData += 1;
    printf("%d, timeout callback\n", *testData);
}
static void (*g_cb)(void*) = Testfun;

HWTEST_F(DfxTest, hitrace_test_normal, TestSize.Level1)
{
#ifdef FFRT_ENABLE_HITRACE_CHAIN
    int HITRACE_FLAG_INCLUDE_ASYNC = 1 << 0;
    const HiTraceIdStruct traceId = TraceChainAdapter::Instance().HiTraceChainBegin("ffrt_dfx_test",
        HITRACE_FLAG_INCLUDE_ASYNC);
    ffrt_task_timeout_set_cb(MyCallback1);
    ffrt_task_timeout_set_threshold(1000);
    FFRT_LOGE("hitrace_test begin");
    std::atomic<std::uint64_t> x{0};
    for (int i = 0; i < 5; i++) {
        ffrt::submit(
            [&]() {
                stall_us(500000);
                x.fetch_add(1);
            }, {}, {});
    }
    ffrt::wait();

    auto testQueue = std::make_unique<queue>("test_queue");
    auto t = testQueue->submit_h([] { stall_us(1100000); FFRT_LOGE("done");}, {});
    testQueue->wait(t);
    FFRT_LOGE("hitrace_test end");
    TraceChainAdapter::Instance().HiTraceChainEnd(&traceId);
    EXPECT_EQ(x, 5);
#endif
}

HWTEST_F(DfxTest, hitrace_test_poller, TestSize.Level1)
{
#ifdef FFRT_ENABLE_HITRACE_CHAIN
    int HITRACE_FLAG_INCLUDE_ASYNC = 1 << 0;
    const HiTraceIdStruct traceId = TraceChainAdapter::Instance().HiTraceChainBegin("ffrt_dfx_test",
        HITRACE_FLAG_INCLUDE_ASYNC);
    LoopPoller poller;
    // 1.组装timeMap_
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
    TraceChainAdapter::Instance().HiTraceChainEnd(&traceId);
#endif
}
