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

#include <random>
#include <csignal>
#include <gtest/gtest.h>
#include "core/entity.h"
#include "core/version_ctx.h"
#include "ffrt_inner.h"
#include "c/ffrt_ipc.h"
#include "sched/task_state.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/bbox/bbox.h"
#include "tm/cpu_task.h"
#include "tm/queue_task.h"
#include "tm/scpu_task.h"
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace ffrt;

class CoreTest : public testing::Test {
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

HWTEST_F(CoreTest, task_ctx_success_01, TestSize.Level1)
{
    auto func1 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    SCPUEUTask *task1 = new SCPUEUTask(nullptr, nullptr, 0, QoS(static_cast<int>(qos_user_interactive)));
    auto func2 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    SCPUEUTask *task2 = new SCPUEUTask(nullptr, task1, 0, QoS());
    QoS qos = QoS(static_cast<int>(qos_inherit));
    task2->SetQos(qos);
    EXPECT_EQ(task2->qos, static_cast<int>(qos_user_interactive));
    delete task1;
    delete task2;
}

HWTEST_F(CoreTest, ffrt_submit_wait_success_01, TestSize.Level1)
{
    const uint32_t sleepTime = 3 * 200;
    int x = 0;
    ffrt_task_attr_t* attr = (ffrt_task_attr_t *) malloc(sizeof(ffrt_task_attr_t));
    ffrt_task_attr_init(attr);
    std::function<void()>&& basicFunc = [&]() {
        usleep(sleepTime);
        x = x + 1;
    };
    ffrt_function_header_t* basicFunc_ht = ffrt::create_function_wrapper((basicFunc));
    const std::vector<ffrt_dependence_t> in_deps = {};
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    const std::vector<ffrt_dependence_t> ou_deps = {};
    ffrt_deps_t ou{static_cast<uint32_t>(ou_deps.size()), ou_deps.data()};
    const std::vector<ffrt_dependence_t> wait_deps = {};
    ffrt_deps_t wait{static_cast<uint32_t>(wait_deps.size()), wait_deps.data()};
    const ffrt_deps_t *wait_null = nullptr;
    ffrt_submit_base(basicFunc_ht, &in, &ou, attr);
    ffrt_wait_deps(wait_null);
    ffrt_wait_deps(&wait);
    ffrt_wait();
    EXPECT_EQ(x, 1);
    ffrt_task_attr_destroy(attr);
    free(attr);
    attr = nullptr;
}

/**
 * @tc.name: ThreadWaitAndNotifyModeCheck
 * @tc.desc: Test function of ThreadWaitMode and ThreadNotifyMode
 * @tc.type: FUNC
 */
HWTEST_F(CoreTest, ThreadWaitAndNotifyMode, TestSize.Level1)
{
    SCPUEUTask* task = new SCPUEUTask(nullptr, nullptr, 0, QoS());

    // when executing task is nullptr
    EXPECT_EQ(ThreadWaitMode(nullptr), true);

    // when executing task is root
    EXPECT_EQ(ThreadWaitMode(task), true);

    // when executing task in legacy mode
    SCPUEUTask* parent = new SCPUEUTask(nullptr, nullptr, 0, QoS());
    task->parent = parent;
    task->legacyCountNum = 1;
    EXPECT_EQ(ThreadWaitMode(task), true);

    // when task is valid and not in legacy mode
    task->legacyCountNum = 0;
    EXPECT_EQ(ThreadWaitMode(task), false);

    // when block thread is false
    EXPECT_EQ(ThreadNotifyMode(task), false);

    // when block thread is true
    task->blockType = BlockType::BLOCK_THREAD;
    EXPECT_EQ(ThreadNotifyMode(task), true);

    delete parent;
    delete task;
}

/**
 * 测试用例名称：task_attr_set_timeout
 * 测试用例描述：验证task_attr的设置timeout接口
 * 预置条件：创建有效的task_attr
 * 操作步骤：设置timeout值，通过get接口与设置值对比
 * 预期结果：设置成功
 */
HWTEST_F(CoreTest, task_attr_set_timeout, TestSize.Level1)
{
    ffrt_task_attr_t* attr = (ffrt_task_attr_t *) malloc(sizeof(ffrt_task_attr_t));
    ffrt_task_attr_init(attr);
    ffrt_task_attr_set_timeout(attr, 1000);
    uint64_t timeout = ffrt_task_attr_get_timeout(attr);
    EXPECT_EQ(timeout, 1000);
    free(attr);
}

/**
 * 测试用例名称：task_attr_set_timeout_nullptr
 * 测试用例描述：验证task_attr的设置timeout接口的异常场景
 * 预置条件：针对nullptr进行设置
 * 操作步骤：设置timeout值，通过get接口与设置值对比
 * 预期结果：设置失败，返回值为0
 */
HWTEST_F(CoreTest, task_attr_set_timeout_nullptr, TestSize.Level1)
{
    ffrt_task_attr_t* attr = nullptr;
    ffrt_task_attr_set_timeout(attr, 1000);
    uint64_t timeout = ffrt_task_attr_get_timeout(attr);
    EXPECT_EQ(timeout, 0);
}

/**
 * 测试用例名称：ffrt_task_handle_ref_nullptr
 * 测试用例描述：验证task_handle的增加、消减引用计数接口的异常场景
 * 预置条件：针对nullptr进行设置
 * 操作步骤：对nullptr进行调用
 * 预期结果：接口校验异常场景成功，用例正常执行结束
 */
HWTEST_F(CoreTest, ffrt_task_handle_ref_nullptr, TestSize.Level1)
{
    ffrt_task_handle_t handle = nullptr;
    ffrt_task_handle_inc_ref(handle);
    ffrt_task_handle_dec_ref(handle);
    EXPECT_EQ(handle, nullptr);
}

/**
 * 测试用例名称：ffrt_task_handle_ref
 * 测试用例描述：验证task_handle的增加、消减引用计数接口
 * 预置条件：创建有效的task_handle
 * 操作步骤：对task_handle进行设置引用计数接口
 * 预期结果：读取rc值
 */
HWTEST_F(CoreTest, ffrt_task_handle_ref, TestSize.Level1)
{
    // 验证notify_worker的功能
    int result = 0;
    ffrt_task_attr_t taskAttr;
    (void)ffrt_task_attr_init(&taskAttr); // 初始化task属性，必须
    ffrt_task_attr_set_delay(&taskAttr, 10000); // 延时10ms执行
    std::function<void()>&& OnePlusFunc = [&result]() { result += 1; };
    ffrt_task_handle_t handle = ffrt_submit_h_base(ffrt::create_function_wrapper(OnePlusFunc), {}, {}, &taskAttr);
    auto task = static_cast<ffrt::CPUEUTask*>(handle);
    EXPECT_EQ(task->rc.load(), 2); // task还未执行完成，所以task和handle各计数一次
    ffrt_task_handle_inc_ref(handle);
    EXPECT_EQ(task->rc.load(), 3);
    ffrt_task_handle_dec_ref(handle);
    EXPECT_EQ(task->rc.load(), 2);
    ffrt::wait({handle});
    EXPECT_EQ(result, 1);
    ffrt_task_handle_destroy(handle);
}

/**
 * 测试用例名称：WaitFailWhenReuseHandle
 * 测试用例描述：构造2个submit_h的任务，验证task_handle转成dependence后，调用ffrt::wait的场景
 * 预置条件：创建一个submit_h任务，确保执行完成，且将task_handle转成dependence后保存
 * 操作步骤：创建另外一个task_handle任务，并且先执行ffrt::wait保存的dependence的数组
 * 预期结果：任务正常执行结束
 */
HWTEST_F(CoreTest, WaitFailWhenReuseHandle, TestSize.Level1)
{
    int i = 0;
    std::vector<ffrt::dependence> deps;
    {
        auto h = ffrt::submit_h([&i] { printf("task0 done\n"); i++;});
        printf("task0 handle: %p\n:", static_cast<void*>(h));
        deps.emplace_back(h);
    }
    usleep(1000);
    std::atomic_bool stop = false;
    auto h = ffrt::submit_h([&] {
        printf("task1 start\n");
        while (!stop);
        i++;
        printf("task1 done\n");
        });
    ffrt::wait(deps);
    EXPECT_EQ(i, 1);
    stop = true;
    ffrt::wait();
    EXPECT_EQ(i, 2);
}

/*
 * 测试用例名称：ffrt_task_get_tid_test
 * 测试用例描述：测试ffrt_task_get_tid接口
 * 预置条件    ：创建SCPUEUTask
 * 操作步骤    ：调用ffrt_task_get_tid方法，入参分别为SCPUEUTask、QueueTask对象和空指针
 * 预期结果    ：ffrt_task_get_tid功能正常，传入空指针时返回0
 */
HWTEST_F(CoreTest, ffrt_task_get_tid_test, TestSize.Level1)
{
    ffrt::CPUEUTask* task = new ffrt::SCPUEUTask(nullptr, nullptr, 0, ffrt::QoS(2));
    ffrt::QueueTask* queueTask = new ffrt::QueueTask(nullptr);
    pthread_t tid = ffrt_task_get_tid(task);
    EXPECT_EQ(tid, 0);

    tid = ffrt_task_get_tid(queueTask);
    EXPECT_EQ(tid, 0);

    tid = ffrt_task_get_tid(nullptr);
    EXPECT_EQ(tid, 0);

    delete task;
    delete queueTask;
}

/*
* 测试用例名称：ffrt_get_cur_cached_task_id_test
* 测试用例描述：测试ffrt_get_cur_cached_task_id接口
* 预置条件    ：设置ExecuteCtx::Cur->lastGid_为自定义值
* 操作步骤    ：调用ffrt_get_cur_cached_task_id接口
* 预期结果    ：ffrt_get_cur_cached_task_id返回值与自定义值相同
*/
HWTEST_F(CoreTest, ffrt_get_cur_cached_task_id_test, TestSize.Level1)
{
    auto ctx = ffrt::ExecuteCtx::Cur();
    ctx->lastGid_ = 15;
    EXPECT_EQ(ffrt_get_cur_cached_task_id(), 15);

    ffrt::submit([] {});
    ffrt::wait();

    EXPECT_NE(ffrt_get_cur_cached_task_id(), 0);
}

/*
* 测试用例名称：ffrt_get_cur_task_test
* 测试用例描述：测试ffrt_get_cur_task接口
* 预置条件    ：提交ffrt任务
* 操作步骤    ：在ffrt任务中调用ffrt_get_cur_task接口
* 预期结果    ：返回的task地址不为空
*/
HWTEST_F(CoreTest, ffrt_get_cur_task_test, TestSize.Level1)
{
    void* taskPtr = nullptr;
    ffrt::submit([&] {
        taskPtr = ffrt_get_cur_task();
    });
    ffrt::wait();

    EXPECT_NE(taskPtr, nullptr);
}

/*
* 测试用例名称：ffrt_this_task_get_qos_test
* 测试用例描述：测试ffrt_this_task_get_qos接口
* 预置条件    ：提交qos=3的ffrt任务
* 操作步骤    ：在ffrt任务中调用ffrt_this_task_get_qos接口
* 预期结果    ：ffrt_this_task_get_qos返回值=3
*/
HWTEST_F(CoreTest, ffrt_this_task_get_qos_test, TestSize.Level1)
{
    ffrt_qos_t qos = 0;
    ffrt::submit([&] {
        qos = ffrt_this_task_get_qos();
    }, ffrt::task_attr().qos(ffrt_qos_user_initiated));
    ffrt::wait();
    EXPECT_EQ(qos, ffrt::QoS(ffrt_qos_user_initiated)());
}