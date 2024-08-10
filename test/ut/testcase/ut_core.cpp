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
#include "sched/task_state.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/bbox/bbox.h"
#include "tm/cpu_task.h"
#include "tm/scpu_task.h"

using namespace std;
using namespace testing;
using namespace testing::ext;
using namespace ffrt;

class CoreTest : public testing::Test {
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

HWTEST_F(CoreTest, core_test_success_01, TestSize.Level1)
{
    sync_io(0);
}

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
        EXPECT_EQ(x, 0);
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
    EXPECT_EQ(x, 0);
    ffrt_wait_deps(wait_null);
    EXPECT_EQ(x, 0);
    ffrt_wait_deps(&wait);
    EXPECT_EQ(x, 0);
    ffrt_wait();
    EXPECT_EQ(x, 1);
    ffrt_task_attr_destroy(attr);
    free(attr);
    attr = nullptr;
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

    ffrt::CPUEUTask* task = new ffrt::SCPUEUTask(nullptr, nullptr, 20, ffrt::QoS(2));
    ctx->task = task;
    EXPECT_NE(ffrt_get_cur_cached_task_id(), 0);
}