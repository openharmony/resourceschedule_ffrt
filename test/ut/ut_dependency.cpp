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
#include "util.h"
#include "c/deadline.h"
#include "c/executor_task.h"
#include "tm/scpu_task.h"
#include "dfx/log/ffrt_log_api.h"
#ifndef WITH_NO_MOCKER
extern "C" int ffrt_set_cgroup_attr(ffrt_qos_t qos, ffrt_os_sched_attr *attr);
#endif
using namespace std;
using namespace testing;
using namespace testing::ext;

class DependencyTest : public testing::Test {
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

HWTEST_F(DependencyTest, dependency_success_01, TestSize.Level1)
{
    int x = 0;
    ffrt::submit([&]() { x = 2; }, {}, {&x});
    ffrt::submit([&]() { x = x * 3; }, {&x}, {});
    ffrt::wait();
    EXPECT_EQ(x, 6);
}

HWTEST_F(DependencyTest, update_qos_success_02, TestSize.Level1)
{
    int ret = ffrt_task_attr_init(nullptr);
    EXPECT_EQ(ret, -1);
    ffrt_task_attr_get_name(nullptr);
    ffrt_task_attr_set_name(nullptr, "A");
    ffrt_task_attr_set_qos(nullptr, static_cast<int>(ffrt::qos_user_initiated));
    ffrt_task_attr_get_qos(nullptr);
    ffrt_task_attr_destroy(nullptr);
    ffrt_submit_base(nullptr, nullptr, nullptr, nullptr);
    ffrt_submit_h_base(nullptr, nullptr, nullptr, nullptr);
    ffrt::submit([] {
        printf("return %d\n", ffrt::this_task::update_qos(static_cast<int>(ffrt::qos_user_initiated)));
        printf("id is  %llu\n", ffrt::this_task::get_id());
    });
    ffrt_this_task_get_id();
    ffrt::wait();
    ffrt_this_task_update_qos(static_cast<int>(ffrt::qos_user_initiated));
#ifndef WITH_NO_MOCKER
    ffrt_set_cgroup_attr(static_cast<int>(ffrt::qos_user_initiated), nullptr);
#endif
}

HWTEST_F(DependencyTest, update_trace_tag_success_02, TestSize.Level1)
{
    ffrt::set_trace_tag("TASK A");
    ffrt::clear_trace_tag();
}

HWTEST_F(DependencyTest, task_attr_success_02, TestSize.Level1)
{
    ffrt::task_attr tmpTask;
    tmpTask.name("Task A");
    tmpTask.qos(static_cast<int>(ffrt::qos_user_initiated));
    tmpTask.name();
    tmpTask.qos();
}

HWTEST_F(DependencyTest, sample_pingpong_pipe_interval_checkpoint, TestSize.Level1)
{
    int loops = 5;
    int frame_num = 2;

    if (getenv("LOOP_NUM")) {
        loops = atoi(getenv("LOOP_NUM"));
    }
    if (getenv("FRAME_NUM")) {
        frame_num = atoi(getenv("FRAME_NUM"));
    }

    ffrt::submit([&]() { stall_us(10); }, {}, {});

    auto it = ffrt::qos_interval_create(16, static_cast<int>(ffrt::qos_user_interactive));
    for (int loop = 0; loop < loops; loop++) {
        constexpr int FRAME_NUM = 3;
        constexpr uint32_t BUFFER_NUM = 2;
        int x0[FRAME_NUM];
        int x1[BUFFER_NUM];
        int x2[BUFFER_NUM];
        int x3[FRAME_NUM];

        int stalls[10][4] = {
            {2000, 6000, 8000, 8000 + 6000 + 2000}, // 0
            {2125, 6375, 8500, 8500 + 6375 + 2125}, // 1
            {2222, 6666, 8888, 8888 + 6666 + 2222}, // 2
            {2250, 6750, 9000, 9000 + 6750 + 2250}, // 3
            {2375, 7125, 9500, 9500 + 7125 + 2375}, // 4
            {2500, 7500, 10000, 10000 + 7500 + 2500}, // 5
            {1875, 5625, 7500, 7500 + 5625 + 1875}, // 6
            {1750, 5250, 7000, 7000 + 5250 + 1750}, // 7
            {1625, 4875, 6500, 6500 + 4875 + 1625}, // 8
            {1500, 4500, 6000, 6000 + 4500 + 1500}, // 9
        };

        auto start = std::chrono::system_clock::now();
        ffrt::qos_interval_begin(it);
        for (int i = 0; i < frame_num; i++) {
            int pingpong = i % BUFFER_NUM;
            // task A
            ffrt::submit(
                [i, loop, stalls]() {
                    FFRT_LOGI("%u", i);
                },
                {x0 + i}, {x1 + pingpong}, ffrt::task_attr().name(("UI" + std::to_string(i)).c_str()));
            // task B
            ffrt::submit(
                [i, loop, stalls]() {
                    FFRT_LOGI("%u", i);
                },
                {x1 + pingpong}, {x2 + pingpong}, ffrt::task_attr().name(("Render" + std::to_string(i)).c_str()));
            // task C
            ffrt::submit(
                [i, loop, stalls]() {
                    FFRT_LOGI("%u", i);
                },
                {x2 + pingpong}, {x3 + i}, ffrt::task_attr().name(("surfaceflinger" + std::to_string(i)).c_str()));
        }
        ffrt::wait();
        ffrt::qos_interval_end(it);
    }

    ffrt::qos_interval_destroy(it);
}
