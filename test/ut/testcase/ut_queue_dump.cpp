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
#include <array>
#include <gtest/gtest.h>
#include <regex>
#include <thread>
#include "c/ffrt_dump.h"
#include "dfx/log/ffrt_log_api.h"
#include "ffrt_inner.h"
#include "securec.h"
#include "util.h"
#include "../common.h"

static const int BUFFER_SIZE = 120000;
static const int SLEEP_MS = 3 * 1000;
static const int TASK_NUM_9 = 9;
static const int TASK_NUM_10 = 10;
static const int TASK_NUM_29 = 29;
static const int TASK_NUM_40 = 40;
static const int TASK_NUM_600 = 600;
static const int TASK_NUM_610 = 610;

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
class QueueDumpTest : public testing::Test {
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

int g_cnt = 0;
std::function<void()>&& basicFunc = []() { g_cnt += 1; };
std::function<void()>&& sleepFunc = []() { std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS)); };

static void QueueDumpTask1Test(ffrt_queue_t& queue_handle, char* buf)
{
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_name(&task_attr, "task1");
    for (int i = 0; i < TASK_NUM_9; ++i) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    }
    ffrt_task_handle_t t =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    ffrt_queue_wait(t);
    EXPECT_EQ(g_cnt, TASK_NUM_10);
    // dump队列信息
    int ret = ffrt_queue_dump(queue_handle, "eventHandler1", buf, BUFFER_SIZE, true);
    EXPECT_TRUE(ret > 0);
    // 预期dump信息:
    // 1.tag为eventHandler1
    // 2.当前执行任务名称为task1
    // 3.有10条历史执行的任务，且任务名称都是task1
    // 4.无剩余未执行的任务
    std::string str(buf);
    std::regex pattern(R"(eventHandler1 Current Running: start at (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task1.*) \}
eventHandler1 History event queue information:
(eventHandler1 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), trigger time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), complete time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task1.*) \}
){10}eventHandler1 Immediate priority event queue information:
eventHandler1 Total size of Immediate events : 0
eventHandler1 High priority event queue information:
eventHandler1 Total size of High events : 0
eventHandler1 Low priority event queue information:
eventHandler1 Total size of Low events : 0
eventHandler1 Idle priority event queue information:
eventHandler1 Total size of Idle events : 0
eventHandler1 Vip priority event queue information:
eventHandler1 Total size of Vip events : 0
eventHandler1 Total event size : 0
)");
    EXPECT_TRUE(std::regex_match(str, pattern));
    ffrt_task_handle_destroy(t);
    ffrt_task_attr_destroy(&task_attr);
}

static void QueueDumpTask2Test(ffrt_queue_t& queue_handle, char* buf)
{
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_name(&task_attr, "task2");
    for (int i = 0; i < TASK_NUM_29; ++i) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    }
    ffrt_task_handle_t t =
        ffrt_queue_submit_h(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    ffrt_queue_wait(t);
    EXPECT_EQ(g_cnt, TASK_NUM_40);
    // dump队列信息
    memset_s(buf, sizeof(char) * BUFFER_SIZE, 0, sizeof(char) * BUFFER_SIZE);
    int ret = ffrt_queue_dump(queue_handle, "eventHandler2", buf, BUFFER_SIZE, true);
    EXPECT_TRUE(ret > 0);
    // 预期dump信息:
    // 1.tag为eventHandler2
    // 2.当前执行任务名称为task2
    // 3.有32条历史执行的任务
    // 4.无剩余未执行的任务
    std::string str(buf);
    std::regex pattern(R"(eventHandler2 Current Running: start at (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task2.*) \}
eventHandler2 History event queue information:
(eventHandler2 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), trigger time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), complete time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = ([^\}]*?) \}
){32}eventHandler2 Immediate priority event queue information:
eventHandler2 Total size of Immediate events : 0
eventHandler2 High priority event queue information:
eventHandler2 Total size of High events : 0
eventHandler2 Low priority event queue information:
eventHandler2 Total size of Low events : 0
eventHandler2 Idle priority event queue information:
eventHandler2 Total size of Idle events : 0
eventHandler2 Vip priority event queue information:
eventHandler2 Total size of Vip events : 0
eventHandler2 Total event size : 0
)");
    EXPECT_TRUE(std::regex_match(str, pattern));
    ffrt_task_handle_destroy(t);
    ffrt_task_attr_destroy(&task_attr);
}

static void QueueDumpTask3Test(ffrt_queue_t& queue_handle, char* buf)
{
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_name(&task_attr, "task3");
    ffrt_queue_submit(queue_handle, create_function_wrapper(sleepFunc, ffrt_function_kind_queue), &task_attr);
    // dump队列信息
    memset_s(buf, sizeof(char) * BUFFER_SIZE, 0, sizeof(char) * BUFFER_SIZE);
    int ret = ffrt_queue_dump(queue_handle, "eventHandler3", buf, BUFFER_SIZE, true);
    EXPECT_TRUE(ret > 0);
    // 预期dump信息:
    // 1.tag为eventHandler3
    // 2.当前执行任务名称为task3
    // 3.有32条历史执行的任务
    // 4.无剩余未执行的任务
    std::string str(buf);
    std::regex pattern(R"(eventHandler3 Current Running: start at (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task3.*) \}
eventHandler3 History event queue information:
(eventHandler3 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), trigger time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), complete time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = ([^\}]*?) \}
){32}eventHandler3 Immediate priority event queue information:
eventHandler3 Total size of Immediate events : 0
eventHandler3 High priority event queue information:
eventHandler3 Total size of High events : 0
eventHandler3 Low priority event queue information:
eventHandler3 Total size of Low events : 0
eventHandler3 Idle priority event queue information:
eventHandler3 Total size of Idle events : 0
eventHandler3 Vip priority event queue information:
eventHandler3 Total size of Vip events : 0
eventHandler3 Total event size : 0
)");
    EXPECT_TRUE(std::regex_match(str, pattern));
    ffrt_task_attr_destroy(&task_attr);
}

static void QueueDumpPriorityTest(ffrt_queue_t& queue_handle, ffrt_task_attr_t& task_attr,
    ffrt_inner_queue_priority_t priority, const char* name)
{
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_queue_priority(&task_attr, static_cast<ffrt_queue_priority_t>(priority));
    ffrt_task_attr_set_name(&task_attr, name);
    for (int i = 0; i < TASK_NUM_10; ++i) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    }
}

static void QueueDumpPriorityTest(ffrt_queue_t& queue_handle, char* buf)
{
    ffrt_task_attr_t task_attr4;
    QueueDumpPriorityTest(queue_handle, task_attr4, ffrt_inner_queue_priority_immediate, "task4");
    ffrt_task_attr_t task_attr5;
    QueueDumpPriorityTest(queue_handle, task_attr5, ffrt_inner_queue_priority_high, "task5");
    ffrt_task_attr_t task_attr6;
    QueueDumpPriorityTest(queue_handle, task_attr6, ffrt_inner_queue_priority_low, "task6");
    ffrt_task_attr_t task_attr7;
    QueueDumpPriorityTest(queue_handle, task_attr7, ffrt_inner_queue_priority_idle, "task7");
    ffrt_task_attr_t task_attr8;
    QueueDumpPriorityTest(queue_handle, task_attr8, ffrt_inner_queue_priority_vip, "task8");
    memset_s(buf, sizeof(char) * BUFFER_SIZE, 0, sizeof(char) * BUFFER_SIZE);
    int ret = ffrt_queue_dump(queue_handle, "eventHandler4", buf, BUFFER_SIZE, true);
    EXPECT_TRUE(ret > 0);
    // 预期dump信息:
    // 1.tag为eventHandler4
    // 2.当前执行任务名称为task3
    // 3.有32条历史执行的任务
    // 4.5种优先级各有10条任务
    std::string str(buf);
    std::regex pattern(R"(eventHandler4 Current Running: start at (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task3.*) \}
eventHandler4 History event queue information:
(eventHandler4 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), trigger time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), complete time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = ([^\}]*?) \}
){32}eventHandler4 Immediate priority event queue information:
(eventHandler4 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task4.*) \}
){10}eventHandler4 Total size of Immediate events : 10
eventHandler4 High priority event queue information:
(eventHandler4 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task5.*) \}
){10}eventHandler4 Total size of High events : 10
eventHandler4 Low priority event queue information:
(eventHandler4 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task6.*) \}
){10}eventHandler4 Total size of Low events : 10
eventHandler4 Idle priority event queue information:
(eventHandler4 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task7.*) \}
){10}eventHandler4 Total size of Idle events : 10
eventHandler4 Vip priority event queue information:
(eventHandler4 No. (\d+) : Event \{ send thread = (\d+), send time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), handle time = (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{3}), task name = (.*task8.*) \}
){10}eventHandler4 Total size of Vip events : 10
eventHandler4 Total event size : 50
)");
    EXPECT_TRUE(std::regex_match(str, pattern));
    ffrt_task_attr_destroy(&task_attr4);
    ffrt_task_attr_destroy(&task_attr5);
    ffrt_task_attr_destroy(&task_attr6);
    ffrt_task_attr_destroy(&task_attr7);
    ffrt_task_attr_destroy(&task_attr8);
}

static void QueueDumpMaxDumpSizeTest(ffrt_queue_t& queue_handle, char* buf)
{
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_attr_set_queue_priority(&task_attr, static_cast<ffrt_queue_priority_t>(ffrt_inner_queue_priority_high));
    ffrt_task_attr_set_name(&task_attr, "task9");
    for (int i = 0; i < TASK_NUM_600; ++i) {
        ffrt_queue_submit(queue_handle, create_function_wrapper(basicFunc, ffrt_function_kind_queue), &task_attr);
    }
    // dump队列信息
    memset_s(buf, sizeof(char) * BUFFER_SIZE, 0, sizeof(char) * BUFFER_SIZE);
    int ret = ffrt_queue_dump(queue_handle, "eventHandler9", buf, BUFFER_SIZE, false);
    EXPECT_TRUE(ret > 0);
    ffrt_task_attr_destroy(&task_attr);
}

/**
 * @brief queue dump interface user cases
 */
HWTEST_F(QueueDumpTest, queue_dump_case, TestSize.Level1)
{
    // 创建类型为ffrt_queue_eventhandler_adapter的队列
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_attr_set_max_concurrency(&queue_attr, 1);
    ffrt_queue_t queue_handle = ffrt_queue_create(static_cast<ffrt_queue_type_t>(ffrt_queue_eventhandler_adapter),
        "queue_dump", &queue_attr);
    char* buf = new char[BUFFER_SIZE];
    // 提交10个任务并等待任务执行完成（任务名称：task1）
    QueueDumpTask1Test(queue_handle, buf);
    // 提交30个任务并等待任务执行完成（任务名称：task2）
    QueueDumpTask2Test(queue_handle, buf);
    // 提交1个睡眠3s的任务（任务名称：task3）
    QueueDumpTask3Test(queue_handle, buf);
    // 5种优先级各提交10个任务
    QueueDumpPriorityTest(queue_handle, buf);
    // 提交600个优先级为ffrt_inner_queue_priority_high的任务
    QueueDumpMaxDumpSizeTest(queue_handle, buf);
    // 验证ffrt_queue_size_dump结果
    EXPECT_EQ(ffrt_queue_size_dump(queue_handle, ffrt_inner_queue_priority_immediate), TASK_NUM_10);
    EXPECT_EQ(ffrt_queue_size_dump(queue_handle, ffrt_inner_queue_priority_high), TASK_NUM_610);
    EXPECT_EQ(ffrt_queue_size_dump(queue_handle, ffrt_inner_queue_priority_low), TASK_NUM_10);
    EXPECT_EQ(ffrt_queue_size_dump(queue_handle, ffrt_inner_queue_priority_idle), TASK_NUM_10);
    EXPECT_EQ(ffrt_queue_size_dump(queue_handle, ffrt_inner_queue_priority_vip), TASK_NUM_10);

    delete[] buf;
    ffrt_queue_destroy(queue_handle);
    ffrt_queue_attr_destroy(&queue_attr);
}
