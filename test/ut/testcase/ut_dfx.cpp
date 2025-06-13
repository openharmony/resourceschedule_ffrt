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
#undef private
#include "c/queue_ext.h"
#include "../common.h"

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

HWTEST_F(DfxTest, queue_dfx_bbox_normal_task_0001, TestSize.Level0)
{
    // 异常信号用例，测试bbox功能正常；
    int x = 0;
    ffrt::mutex lock;

    pid_t pid = fork();
    if (!pid) {
        printf("pid = %d, thread id= %d  start\n", getpid(), pid);

        auto basic1Func = [&]() {
            lock.lock();
            ffrt_usleep(2000);
            x = x + 1;
            lock.unlock();
        };

        auto basic2Func = [&]() {
            ffrt_usleep(3000);
            SignalReg(SIGABRT);
            raise(SIGABRT);  // 向自身进程发送SIGABR
        };

        auto basic3Func = [&]() {
            ffrt_usleep(5000);
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

HWTEST_F(DfxTest, queue_dfx_bbox_queue_task_0001, TestSize.Level0)
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
* 测试用例名称：dfx_bbox_normal_task_0002
* 测试用例描述：提交有依赖关系的任务，测试SaveTheBbox和RecordDebugInfo函数
* 预置条件    ：无
* 操作步骤    ：提交嵌套任务，子任务等待cv和数据依赖，调用SaveTheBbox和RecordDebugInfo接口
* 预期结果    ：SaveTheBbox()函数正确执行
*/
HWTEST_F(DfxTest, dfx_bbox_normal_task_0002, TestSize.Level0)
{
    SaveTheBbox();
    ffrt::submit([]() {
        RecordDebugInfo();
    });
    ffrt::wait();
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
    int x = 0;
    for (int i = 0; i < 3; i++) {
        ffrt::submit(
            [&]() {
                stall_us(2000000);
                x++;
            }, {}, {});
    }
    ffrt::wait();

    EXPECT_EQ(x, 3);
}

/*
* 测试用例名称：normaltsk_timeout_multi
* 测试用例描述：多个并发任务WorkerMonitor检测超时
* 操作步骤    ：1、提交多个执行时间长任务
* 预期结果    ：正确触发多次超时告警
*/
HWTEST_F(DfxTest, normaltsk_timeout_multi, TestSize.Level1)
{
    ffrt_task_timeout_set_threshold(1000);
    int x = 0;
    for (int i = 0; i < 5; i++) {
        ffrt::submit(
            [&]() {
                stall_us(1500000);
                x++;
            }, {}, {});
    }
    ffrt::wait();
    EXPECT_EQ(x, 5);
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
    int x = 0;
    for (int i = 0; i < 2; i++) {
        ffrt::submit([&]() {
            std::cout << "delay " << 1500 << " us\n";
            x = x + 1;
            stall_us(1500000);
        }, {}, {}, ffrt::task_attr().delay(1500000));
    }
    ffrt::wait();
    EXPECT_EQ(x, 2);
    ffrt::WorkerMonitor::GetInstance().timeoutUs_ = 30000000;
}