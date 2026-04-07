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
#include <regex>
#include <climits>
#include "securec.h"
#include "ffrt_inner.h"
#include "dfx/bbox/bbox.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#define private public
#include "util/worker_monitor.h"
#include "eu/loop_poller.h"
#include "queue/queue_monitor.h"
#undef private
#include "c/queue_ext.h"
#include "../common.h"
#ifdef FFRT_ENABLE_HITRACE_CHAIN
#include "dfx/trace/ffrt_trace_chain.h"
#endif
using namespace ffrt;

extern void SaveTheBbox();
extern void RecordDebugInfo();
static const int g_logBufferSize = 120000;

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

std::string getSelfExePath()
{
    char path[PATH_MAX] = {0};
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        return std::string(path);
    }
    return "";
}

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

/*
* 测试用例名称：FfrtSetTaskTimeoutCallback
* 测试用例描述：FFRT能正常设置并获取任务超时回调
* 操作步骤    ：1. 使用ffrt_task_attr_set_timeout_callback/callback(const std::function<void()>& func)接口设置超时回调
*              2.使用ffrt_task_attr_get_timeout_callback/ callback()获取回调
*              3. 使用get到的函数指针执行对应函数，校验结果
*              4.ffrt_task_attr_set_timeout_callback/callback(const std::function<void()>& func)接口重新设置超时回调（对同一个attr）
*              5.使用ffrt_task_attr_get_timeout_callback/ callback()获取回调
*              6. 使用get到的函数指针执行对应函数，校验结果
* 预期结果    ：1、所有校验均成功
*/
HWTEST_F(DfxTest, ffrt_set_timeout_callback_test, TestSize.Level0)
{
    int x = 0;
    std::function<void()>&& ScheduleTimeoutCallBack = [&]() {
        std::cout << "timeout execute!" << std::endl;
        x++;
    };
    ffrt_task_attr_t cAttr;
    ffrt_task_attr_init(&cAttr);
    ffrt_task_attr_set_timeout_callback(&cAttr,
        ffrt::create_function_wrapper(ScheduleTimeoutCallBack, ffrt_function_kind_general));
    auto cf = ffrt_task_attr_get_timeout_callback(&cAttr);
    cf->exec(cf);
    ffrt_task_attr_destroy(&cAttr);

    ffrt::task_attr cppAttr;
    cppAttr.timeout_callback(ScheduleTimeoutCallBack);
    auto cppf = cppAttr.timeout_callback();
    cppf->exec(cppf);

    EXPECT_EQ(x, 2);
}

HWTEST_F(DfxTest, ffrt_dfx_process_test, TestSize.Level0)
{
    printf("Parent process [%d]: starting serial child test\n", getpid());
    std::vector<std::string> testCases = {
        "--gtest_filter=DfxTest.water_line_test",
        "--gtest_filter=DfxTest.ffrt_queue_dfx_timeout",
        "--gtest_filter=DfxTest.ffrt_worker_dfx_timeout"
    };
    bool allSuccess = true;
    for (const auto& testCase : testCases) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            allSuccess = false;
            continue;
        }
        if (pid == 0) {
            std::string self = getSelfExePath();
            if (self.empty()) {
                fprintf(stderr, "Child: failed to get self path\n");
                _exit(1);
            }
            setenv("FFRT_RUN_ONLY_DFX_TEST", "1", 1);
            printf("Child [%d]: exec to run %s\n", getpid(), testCase.c_str());
            execl(self.c_str(),
                    "ffrt_ut",
                    testCase.c_str(),
                    static_cast<char*>(nullptr));
            perror("execl failed");
            _exit(1);
        }

        int status;
        pid_t waited_pid = waitpid(pid, &status, 0);
        if (waited_pid == -1) {
            perror("waitpid failed");
            allSuccess = false;
            continue;
        }

        printf("Parent: Child %d (test case: %s) has finished.\n", waited_pid, testCase.c_str());
        if (WIFEXITED(status)) {
            int exitCode = WEXITSTATUS(status);
            printf("Child %d exited with code %d\n", waited_pid, exitCode);
            if (exitCode != 0) {
                allSuccess = false;
            }
        } else {
            printf("Child %d terminated abnormally\n", waited_pid);
            allSuccess = false;
        }
    }
    EXPECT_EQ(true, allSuccess);
}

/*
* 用例名称：FftsTaskOverWaterLineBySubmit
* 用例描述：FFTS资源被submit后维测是否正常
* 操作步骤：
*   1.提交10个通路一协程睡眠3s任务；
*   2.提交FFTS_NORMAL_WORKER_CNT_WATERLINE-10个通路一正常任务，输入依赖为上一个提交的睡眠任务；
*   3.测试线程睡眠1秒，等待任务全部调度起来
*   4.校验FFRTTraceRecord::g_recordTaskTime_[ffrt_normal_task][2]中的CoCounterInSwitch是否为10
*   5.等待所有任务执行结束
*   6.校验提交任务是否正常执行
*   7.校验FFRTTraceRecord::g_recordTaskTime_[ffrt_normal_task][2]中的CoCounterInSwitch是否为0
* 预期结果：校验通过
*/
HWTEST_F(DfxTest, water_line_test, TestSize.Level0)
{
    if (getenv("FFRT_RUN_ONLY_DFX_TEST") == nullptr) {
        return;
    }
    int switchNum = 10;
    std::atomic<int> x{0};
    ffrt::task_handle sleepHandle;
    for (int i = 0; i < switchNum; i++) {
        sleepHandle = ffrt::submit_h([]() { ffrt::this_task::sleep_for(std::chrono::seconds(3)); });
    }
    ffrt::this_task::sleep_for(std::chrono::seconds(1));
    EXPECT_EQ(ffrt::FFRTTraceRecord::g_recordTaskCounter_[0][2].CoCounterInSwitch.load(), switchNum);
    ffrt::wait();
    EXPECT_EQ(ffrt::FFRTTraceRecord::g_recordTaskCounter_[0][2].CoCounterInSwitch.load(), 0);
    _exit(0);
}

#ifdef FFRT_WORKER_MONITOR
HWTEST_F(DfxTest, ffrt_queue_dfx_timeout, TestSize.Level0)
{
    if (getenv("FFRT_RUN_ONLY_DFX_TEST") == nullptr) {
        return;
    }
    uint32_t timeoutMs = ffrt_task_timeout_get_threshold();
    EXPECT_EQ(timeoutMs, 30000);
    ffrt_task_timeout_set_threshold(1500);
    timeoutMs = ffrt_task_timeout_get_threshold();
    EXPECT_EQ(timeoutMs, 1500);
    ffrt::QueueMonitor::GetInstance().suspendAlarm_ = true;
    ffrt::queue serialQueue = ffrt::queue("FfrtQueueChangeDfxTimeout", {});
    ffrt::task_handle handle = serialQueue.submit_h([&]() {
        stall_us(3000 * 1000);
    });
    EXPECT_EQ(ffrt::QueueMonitor::GetInstance().timeoutUs_, 1500 * 1000);
    serialQueue.wait(handle);
    _exit(0);
}

/*
* 测试用例名称：FfrtWorkerChangeDfxTimeout
* 测试用例描述：运行时改变Worker HivewDfx的超时时间，等待一会后查看超时时间修改是否生效
* 操作步骤    ：0. 该用例需要fork一个单独的进程
*              1. 校验ffrt_task_timeout_get_threshold接口返回值是否为30000默认值；
*              2.使用ffrt_task_timeout_set_threshold接口设置超时时长为1.5s
*              3.校验ffrt_task_timeout_get_threshold接口返回值是否为1500（1.5s）
*              4.提交一个通路一自旋3秒的任务
*              5.校验WorkerMonitor的timeoutUs_变量是否被修改为1500000
*              6.等待所有任务结束
* 预期结果    ：1、所有校验均成功
*/
HWTEST_F(DfxTest, ffrt_worker_dfx_timeout, TestSize.Level0)
{
    if (getenv("FFRT_RUN_ONLY_DFX_TEST") == nullptr) {
        return;
    }
    uint32_t timeoutMs = ffrt_task_timeout_get_threshold();
    EXPECT_EQ(timeoutMs, 30000);
    ffrt_task_timeout_set_threshold(1500);
    timeoutMs = ffrt_task_timeout_get_threshold();
    EXPECT_EQ(timeoutMs, 1500);
    ffrt::submit([&]() {
        stall_us(3000 * 1000);
    });
    EXPECT_EQ(ffrt::WorkerMonitor::GetInstance().timeoutUs_, 1500 * 1000);
    ffrt::wait();
    _exit(0);
}
#endif

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

#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_2)
HWTEST_F(DfxTest, task_statistic_info_dump, TestSize.Level1)
{
    char* buf = new char[g_logBufferSize];
    for (int i = 0; i < 100; i++) {
        ffrt::submit([&]() {ffrt_usleep(10 * 1000);}, task_attr().name(("taskA" + std::to_string(i)).c_str())
            .qos(qos_user_interactive));
    }
    ffrt::wait();
    int ret = ffrt_dump(DUMP_TASK_STATISTIC_INFO, buf, g_logBufferSize);
    EXPECT_TRUE(ret > 0);
    std::string str1(buf);
    std::cout << str1 << std::endl;
#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_3)
    std::regex pattern1(R"(---
Qos TaskType SubmitNum EnueueNum CoSwitchNum   DoneNum FinishNum MaxWorkerNum MaxWaitTime\(us\)
    MaxRunDuration\(us\) AvgWaitTime\(us\) avgRunDuration\(us\) TotalWaitTime\(us\) TotalRunnDuration\(us\)
    5   normal       100       100         100       100       100    (\s*\d+)*
---
)");
#else
    std::regex pattern1(R"(---
Qos TaskType SubmitNum EnueueNum CoSwitchNum   DoneNum FinishNum
    5   normal       100       100         100       100       100
---
)");
#endif
    // 预期dump信息:
    // 1.qos 5任务提交数量为100、入队数量为100、协程切换数量为100、任务完成数量为100、任务终止数量为100
    EXPECT_TRUE(std::regex_match(str1, pattern1));

    for (int i = 0; i < 100; i++) {
        ffrt::submit([&]() {std::this_thread::sleep_for(10ms);}, task_attr().name(("taskB" + std::to_string(i)).c_str())
            .qos(qos_deadline_request));
    }
    ffrt::wait();
    memset_s(buf, sizeof(char) * g_logBufferSize, 0, sizeof(char) * g_logBufferSize);
    ret = ffrt_dump(DUMP_TASK_STATISTIC_INFO, buf, g_logBufferSize);
    std::string str2(buf);
#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_3)
    std::regex pattern2(R"(---
Qos TaskType SubmitNum EnueueNum CoSwitchNum   DoneNum FinishNum MaxWorkerNum MaxWaitTime\(us\)
    MaxRunDuration\(us\) AvgWaitTime\(us\) avgRunDuration\(us\) TotalWaitTime\(us\) TotalRunnDuration\(us\)
    4   normal       100       100           0       100       100    (\s*\d+)*
    5   normal       100       100         100       100       100    (\s*\d+)*
---
)");
#else
    std::regex pattern2(R"(---
Qos TaskType SubmitNum EnueueNum CoSwitchNum   DoneNum FinishNum
    4   normal       100       100           0       100       100
    5   normal       100       100         100       100       100
---
)");
#endif
    // 预期dump信息:
    // 1.qos 4任务提交数量为100、入队数量为100、协程切换数量为0、任务完成数量为100、任务终止数量为100
    // 2.qos 5任务提交数量为100、入队数量为100、协程切换数量为100、任务完成数量为100、任务终止数量为100
    // 3.提交计数为200
    // 4.入队计数为200
    // 5.运行计数为300
    // 6.完成计数为200
    // 7.协程切换计数为100
    // 8.终止计数为200
    EXPECT_TRUE(std::regex_match(str2, pattern2));
    EXPECT_EQ(ffrt::FFRTTraceRecord::GetSubmitCount(), 200);
    EXPECT_EQ(ffrt::FFRTTraceRecord::GetEnqueueCount(), 200);
    EXPECT_EQ(ffrt::FFRTTraceRecord::GetRunCount(), 300);
    EXPECT_EQ(ffrt::FFRTTraceRecord::GetDoneCount(), 200);
    EXPECT_EQ(ffrt::FFRTTraceRecord::GetCoSwitchCount(), 100);
    EXPECT_EQ(ffrt::FFRTTraceRecord::GetFinishCount(), 200);
    delete[] buf;
}
#endif
