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
#include "dfx/bbox/bbox.h"
#include "c/queue_ext.h"
#include "../common.h"

using namespace ffrt;

extern void SaveTheBbox();

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

    virtual void SetUp()
    {
    }

    virtual void TearDown()
    {
    }
};

HWTEST_F(DfxTest, bboxtest, TestSize.Level1)
{
    SaveTheBbox();
}

HWTEST_F(DfxTest, tracetest, TestSize.Level1)
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

HWTEST_F(DfxTest, queue_dfx_bbox_normal_task_0001, TestSize.Level1)
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

HWTEST_F(DfxTest, queue_dfx_bbox_queue_task_0001, TestSize.Level1)
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
