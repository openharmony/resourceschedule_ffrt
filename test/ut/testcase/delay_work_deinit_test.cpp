/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "c/ffrt_dump.h"
#include "sync/delayed_worker.h"
#include "../common.h"
#include "util/ffrt_facade.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class DelayWorkDeinitTest : public testing::Test {
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

void CheckCallBackThreadName()
{
    EXPECT_EQ(true, DelayedWorker::IsDelayerWorkerThread());
}

WaitUntilEntry g_delayWorkerThreadTestWe;

void SendDelayedWorker(uint64_t timeoutUs)
{
    TimePoint now = std::chrono::steady_clock::now();
    TimePoint delay = now + std::chrono::microseconds(timeoutUs);

    g_delayWorkerThreadTestWe.tp = delay;
    g_delayWorkerThreadTestWe.cb = ([](ffrt::WaitEntry* we) { CheckCallBackThreadName(); });
    FFRTFacade::GetDWInstance().dispatch(g_delayWorkerThreadTestWe.tp,
        &g_delayWorkerThreadTestWe, g_delayWorkerThreadTestWe.cb);
}

/*
 * 测试用例名称 : delay_work_thread_para_01
 * 测试用例描述：测试delayWork线程变量是否正确配置
 * 操作步骤    ：1、提交DelayWorker任务
 *              2、等待任务执行完成
 * 预期结果    ：检测是否为delayworker线程，callback函数中检查为是，主线程检查为否
 */
HWTEST_F(DelayWorkDeinitTest, delay_work_thread_para, TestSize.Level0)
{
    const uint64_t timeoutUs = 100;
    DelayedWorker::ThreadEnvCreate();
    SendDelayedWorker(timeoutUs);
    sleep(1);
    EXPECT_EQ(false, DelayedWorker::IsDelayerWorkerThread());
}