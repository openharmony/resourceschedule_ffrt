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
#include <random>
#include <cinttypes>
#include "ffrt_inner.h"
#include "util.h"
#include "c/deadline.h"
#include "c/executor_task.h"
#include "tm/scpu_task.h"
#include "dfx/log/ffrt_log_api.h"
#define private public
#include "dm/sdependence_manager.h"
#include "sched/task_scheduler.h"
#undef private
#include "util/ffrt_facade.h"
#ifndef WITH_NO_MOCKER
extern "C" int ffrt_set_cgroup_attr(ffrt_qos_t qos, ffrt_os_sched_attr *attr);
#endif
#include "../common.h"

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class DependencyTest : public testing::Test {
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

HWTEST_F(DependencyTest, dependency_success, TestSize.Level0)
{
    int x = 0;
    ffrt::submit([&]() { x = 2; }, {}, {&x});
    ffrt::submit([&]() { x = x * 3; }, {&x}, {});
    ffrt::wait();
    EXPECT_EQ(x, 6);
}

HWTEST_F(DependencyTest, update_qos_success, TestSize.Level0)
{
    int ret = ffrt_task_attr_init(nullptr);
    EXPECT_EQ(ret, -1);
    ffrt::submit([] {
        printf("return %d\n", ffrt::this_task::update_qos(static_cast<int>(ffrt::qos_user_initiated)));
        printf("id is  %" PRIu64 "\n", ffrt::this_task::get_id());
    });
    ffrt::this_task::get_id();
    ffrt_restore_qos_config();
    ffrt::wait();
    ffrt_this_task_update_qos(static_cast<int>(ffrt::qos_user_initiated));
#ifndef WITH_NO_MOCKER
    ffrt_set_cgroup_attr(static_cast<int>(ffrt::qos_user_initiated), nullptr);
#endif
}

HWTEST_F(DependencyTest, update_qos_failed, TestSize.Level0)
{
    int x = 0;
    int ret = ffrt_task_attr_init(nullptr);
    EXPECT_EQ(ret, -1);
    ffrt_task_attr_get_name(nullptr);
    ffrt_task_attr_set_name(nullptr, "A");
    ffrt_task_attr_set_qos(nullptr, static_cast<int>(ffrt::qos_user_initiated));
    ffrt_task_attr_get_qos(nullptr);
    ffrt_task_attr_destroy(nullptr);
    ffrt_submit_base(nullptr, nullptr, nullptr, nullptr);
    ffrt_submit_h_base(nullptr, nullptr, nullptr, nullptr);
    ffrt::submit([&] {
        printf("return %d\n", ffrt::this_task::update_qos(static_cast<int>(ffrt::qos_user_initiated)));
        printf("id is  %" PRIu64 "\n", ffrt::this_task::get_id());
        int ret1 = ffrt_this_task_update_qos(static_cast<int>(ffrt::qos_default));
        EXPECT_EQ(ret1, 0);
        x++;
    });
    ffrt_this_task_get_id();
    ffrt::wait();
#ifndef WITH_NO_MOCKER
    ffrt_set_cgroup_attr(static_cast<int>(ffrt::qos_user_initiated), nullptr);
#endif
    EXPECT_EQ(x, 1);
}

HWTEST_F(DependencyTest, set_worker_max_num_invaild_qos, TestSize.Level0)
{
    int ret = ffrt_task_attr_init(nullptr);
    EXPECT_EQ(ret, -1);
    ffrt::submit([] {
        printf("return %d\n", ffrt::this_task::update_qos(static_cast<int>(ffrt::qos_user_initiated)));
    });
    int ret1 = ffrt_set_cpu_worker_max_num(static_cast<int>(ffrt::qos_inherit), 4);
    EXPECT_EQ(ret1, -1);
    ffrt::wait();
}

/*
 * 测试用例名称 ：set_worker_min_num_test
 * 测试用例描述 ：测试传入0个线程的情况
 * 操作步骤     ：测试传入0个线程
 * 预期结果     ：预期失败
 */
HWTEST_F(DependencyTest, set_worker_min_num_test, TestSize.Level0)
{
    int ret = ffrt_task_attr_init(nullptr);
    EXPECT_EQ(ret, -1);
    ffrt::submit([] {
        printf("return %d\n", ffrt::this_task::update_qos(static_cast<int>(ffrt::qos_user_initiated)));
    });
    int ret2 = ffrt_set_cpu_worker_max_num(static_cast<int>(ffrt::qos_user_initiated), 0);
    EXPECT_EQ(ret2, -1);
    ffrt::wait();
}

/*
 * 测试用例名称 ：set_worker_max_num_test
 * 测试用例描述 ：测试传入超过最大线程数量的情况
 * 操作步骤     ：测试传入160个线程
 * 预期结果     ：预期失败
 */
HWTEST_F(DependencyTest, set_worker_max_num_test, TestSize.Level0)
{
    int ret = ffrt_task_attr_init(nullptr);
    EXPECT_EQ(ret, -1);
    ffrt::submit([] {
        printf("return %d\n", ffrt::this_task::update_qos(static_cast<int>(ffrt::qos_user_initiated)));
    });
    int ret1 = ffrt_set_cpu_worker_max_num(static_cast<int>(ffrt::qos_user_initiated), 160);
    EXPECT_EQ(ret1, -1);
    ffrt::wait();
}

HWTEST_F(DependencyTest, ffrt_task_attr_get_name_set_notify_test, TestSize.Level0)
{
    int x = 0;
    int ret = ffrt_task_attr_init(nullptr);
    EXPECT_EQ(ret, -1);
    ffrt_task_attr_t attr;
    attr.storage[0] = 12345;
    ffrt_task_attr_set_name(nullptr, "A");
    ffrt_task_attr_get_name(&attr);
    ffrt_task_attr_set_notify_worker(&attr, true);
    ffrt_task_attr_set_notify_worker(nullptr, true);
    ffrt_task_attr_set_qos(nullptr, static_cast<int>(ffrt::qos_user_initiated));
    ffrt_task_attr_get_qos(nullptr);
    ffrt_task_attr_destroy(nullptr);
    ffrt_submit_base(nullptr, nullptr, nullptr, nullptr);
    ffrt_submit_h_base(nullptr, nullptr, nullptr, nullptr);
    ffrt::submit([&] {
        printf("return %d\n", ffrt::this_task::update_qos(static_cast<int>(ffrt::qos_user_initiated)));
        printf("id is %" PRIu64 "\n", ffrt::this_task::get_id());
        int ret1 = ffrt_this_task_update_qos(static_cast<int>(ffrt::qos_default));
        EXPECT_EQ(ret1, 0);
        x++;
    });
    ffrt_this_task_get_id();
    ffrt::wait();
    ffrt_set_cgroup_attr(static_cast<int>(ffrt::qos_user_initiated), nullptr);
    EXPECT_EQ(x, 1);
}

HWTEST_F(DependencyTest, executor_task_submit_success_cancel, TestSize.Level0)
{
    ffrt_task_attr_t attr;
    static ffrt_executor_task_t work;
    work.wq[0] = &work.wq;
    work.wq[1] = &work.wq;
    work.type = reinterpret_cast<uintptr_t>(&attr);
    ffrt_executor_task_submit(nullptr, nullptr);

    ffrt_executor_task_submit(&work, &attr);

    int ret = ffrt_executor_task_cancel(nullptr, static_cast<int>(ffrt::qos_user_initiated));
    EXPECT_EQ(ret, 0);
}

HWTEST_F(DependencyTest, executor_task_submit_delay_cancel, TestSize.Level0)
{
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_executor_task_t work;
    work.type = reinterpret_cast<uintptr_t>(&attr);

    ffrt_task_attr_set_qos(&attr, static_cast<int>(ffrt::qos_user_initiated));
    ffrt_executor_task_submit(&work, &attr);
    usleep(10000);
    int cancelled = ffrt_executor_task_cancel(&work, static_cast<int>(ffrt::qos_user_initiated));
    EXPECT_EQ(cancelled, 0);

    ffrt_task_attr_destroy(&attr);
}

static std::atomic_int uv_sleep = 0;
void ffrt_work_sleep(ffrt_executor_task_t* data, ffrt_qos_t qos)
{
    usleep(1000);
    uv_sleep++;
}

static void init_once_sleep(void)
{
    uv_sleep = 0;
    ffrt_executor_task_register_func(ffrt_work_sleep, ffrt_uv_task);
}

int ThreadSafeRand(const int min, const int max)
{
    /* note that std:mt19937 is not thread-safe,
     * so each thread has to has its own instance,
     * or it should be protected by a lock */
    static thread_local std::mt19937 generator {std::random_device{}()};
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(generator);
}

/*
* 测试用例名称：executor_task_submit_cancel_uv_tasks
* 测试用例描述：uv任务正确取消
* 预置条件    ：无
* 操作步骤    ：1、调用注册接口，给uv任务注册一个耗时函数
               2、提交taskCount个任务
               3、随机取消taskCount次任务
* 预期结果    ：1、能够取消至少一个uv任务
               2、取消的任务数+已执行的任务数=总提交任务数
               3、取消的任务数>0，已执行的任务数<总提交任务数
*/
HWTEST_F(DependencyTest, executor_task_submit_cancel_uv_tasks, TestSize.Level0)
{
    const int taskCount = 10000;
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    static ffrt_executor_task_t work[taskCount];
    init_once_sleep();
    std::atomic_int cancelCount {0};
    ffrt::task_attr task_attr;
    task_attr.qos(ffrt::qos_background);
    ffrt_task_attr_set_qos(&attr, static_cast<int>(ffrt::qos_user_initiated));
    auto tryCancel = [&]() {
        for (int i = taskCount - 1; i >= 0; --i) {
            auto idx = ThreadSafeRand(0, taskCount - 1);
            cancelCount += ffrt_executor_task_cancel(&work[idx], static_cast<int>(ffrt::qos_user_initiated));
        }
    };
    for (int i = 0; i < taskCount; i++) {
        work[i].type = ffrt_uv_task;
        ffrt_executor_task_submit(&work[i], &attr);
    }
    ffrt::submit(tryCancel, {}, {}, task_attr);
    ffrt::submit(tryCancel, {}, {}, task_attr);
    ffrt::wait();
    sleep(2);
    EXPECT_LT(uv_sleep, taskCount);
    EXPECT_GT(uv_sleep, 0);
    EXPECT_GE(uv_sleep + cancelCount, taskCount);
    ffrt_task_attr_destroy(&attr);
}

namespace {
std::atomic_int uv_result = 0;
std::mutex uv_cancel_mtx;
std::condition_variable uv_cancel_cv;
// libuv's uv__queue
struct UvQueue {
    struct UvQueue* next;
    struct UvQueue* prev;
};

// libuv's uv__queue_empty
inline int UvQueueEmpty(const struct UvQueue* q)
{
    std::lock_guard lg(ffrt::FFRTFacade::GetSchedInstance()->GetScheduler(ffrt::qos_user_initiated).uvMtx);
    return q == q->next || q != q->next->prev;
}

void UVCbSleep(ffrt_executor_task_t* data, ffrt_qos_t qos)
{
    (void)data;
    (void)qos;
    uv_result.fetch_add(1);
    std::unique_lock lk(uv_cancel_mtx);
    uv_cancel_cv.wait(lk);
}
}

/*
 * 测试用例名称 ：executor_task_cancel_with_uv_queue
 * 测试用例描述 ：测试通过libuv视角，能够正确取消任务
 * 操作步骤     ：1.提交批量任务，让worker都阻塞，使得所有uv任务都不从Ready中取出
 *               2.判断uv__queue_empty位false时，调用ffrt_executor_task_cancel取消任务
 * 预期结果     ：能够至少取消一个任务，并且任务执行和取消等于总任务数
 */
HWTEST_F(DependencyTest, executor_task_cancel_with_uv_queue, TestSize.Level1)
{
    uv_result.store(0);
    constexpr int taskCount = 10000;
    std::atomic_int cancelCount = 0;
    std::atomic<bool> flag = false;
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_qos(&attr, static_cast<int>(ffrt::qos_user_initiated));
    ffrt_executor_task_register_func(UVCbSleep, ffrt_uv_task);
    ffrt_executor_task_t works[taskCount];

    for (int i = 0; i < taskCount; i++) {
        ffrt_executor_task_submit(&works[i], &attr);
    }
    while (uv_result < 1) {
        usleep(100);
    }
    thread {[&]() {
        for (int i = 0; i < taskCount; i++) {
            if (!UvQueueEmpty(reinterpret_cast<UvQueue*>(&works[i].wq)) &&
                ffrt_executor_task_cancel(&works[i], ffrt::qos_user_initiated)) {
                cancelCount.fetch_add(1);
                flag.store(true, std::memory_order_relaxed);
            }
        }
    }}.detach();
    while (!flag.load(std::memory_order_relaxed)) {
        usleep(100);
    }
    while (uv_result + cancelCount < taskCount) {
        usleep(1000);
        uv_cancel_cv.notify_all();
    }
    printf("canceled: %d, result: %d\n", cancelCount.load(), uv_result.load());
    EXPECT_GT(cancelCount, 0);
    EXPECT_EQ(cancelCount.load() + uv_result.load(), taskCount);
}

HWTEST_F(DependencyTest, update_trace_tag_task_attr_success, TestSize.Level1)
{
    ffrt::set_trace_tag("TASK A");
    ffrt::clear_trace_tag();

    ffrt::task_attr tmpTask;
    tmpTask.name("Task A");
    tmpTask.qos(static_cast<int>(ffrt::qos_user_initiated));

    EXPECT_EQ(ffrt_task_attr_get_qos(&tmpTask), ffrt::qos_user_initiated);
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
                [i, loop, stalls, &x0, &x1]() {
                    FFRT_LOGI("%u", i);
                    x1[i % BUFFER_NUM] = i;
                },
                {x0 + i}, {x1 + pingpong}, ffrt::task_attr().name(("UI" + std::to_string(i)).c_str()));
            // task B
            ffrt::submit(
                [i, loop, stalls, &x1, &x2]() {
                    FFRT_LOGI("%u", i);
                    x2[i % BUFFER_NUM] = i;
                },
                {x1 + pingpong}, {x2 + pingpong}, ffrt::task_attr().name(("Render" + std::to_string(i)).c_str()));
            // task C
            ffrt::submit(
                [i, loop, stalls, &x2, &x3]() {
                    FFRT_LOGI("%u", i);
                    x3[i] = i;
                },
                {x2 + pingpong}, {x3 + i}, ffrt::task_attr().name(("surfaceflinger" + std::to_string(i)).c_str()));
        }
        ffrt::wait();
        ffrt::qos_interval_end(it);

        for (int i = 0; i < frame_num; i++) {
            EXPECT_EQ(x3[i], i) << "Task C result is incorrect for frame " << i;
        }
    }

    ffrt::qos_interval_destroy(it);
}

void AddOne(void* args)
{
    *(static_cast<int*>(args)) += 1;
}

namespace {
ffrt::mutex uv_block_mtx;
std::atomic_int uv_block_result = 0;
const unsigned int UV_BLOCK_SLEEP_TIME = 10;
void UVBlockCb(ffrt_executor_task_t* data, ffrt_qos_t qos)
{
    (void)qos;
    std::unique_lock lk(uv_block_mtx);
    usleep(UV_BLOCK_SLEEP_TIME);
    uv_block_result.fetch_add(1);
}
int Rand()
{
        std::random_device rd;
        std::mt19937 gen(rd());
        constexpr int min = 1;
        constexpr int max = 20;
        std::uniform_int_distribution<int> dis(min, max);
        return dis(gen);
}
}

HWTEST_F(DependencyTest, uv_task_block_ffrt_mutex, TestSize.Level0)
{
    ffrt_executor_task_register_func(UVBlockCb, ffrt_uv_task);
    const int taskCount = 4000;
    auto submitNormal = []() {
        for (int qos = ffrt_qos_background; qos <= ffrt_qos_user_initiated; qos++) {
            ffrt::submit([]() {
                usleep(1);
            }, {}, {}, ffrt::task_attr().qos(qos));
            usleep(Rand());
        }
    };
    auto t1 = std::thread{[submitNormal]() {
        for (int i = 0; i < taskCount; i++) {
            if (uv_block_result >= taskCount * 4) {
                break;
            }
            submitNormal();
        }
        ffrt::wait();
    }};
    ffrt_executor_task_t uvWork[taskCount * 4];
    auto t2 = std::thread{[&]() {
        ffrt_task_attr_t attr;
        ffrt_task_attr_init(&attr);
        for (int i = 0; i < taskCount; i++) {
            ffrt_task_attr_set_qos(&attr, ffrt_qos_background);
            ffrt_executor_task_submit(&uvWork[i * 4], &attr);
            usleep(Rand());
            ffrt_task_attr_set_qos(&attr, ffrt_qos_utility);
            ffrt_executor_task_submit(&uvWork[i * 4 + 1], &attr);
            usleep(Rand());
            ffrt_task_attr_set_qos(&attr, ffrt_qos_default);
            ffrt_executor_task_submit(&uvWork[i * 4 + 2], &attr);
            usleep(Rand());
            ffrt_task_attr_set_qos(&attr, ffrt_qos_user_initiated);
            ffrt_executor_task_submit(&uvWork[i * 4 + 3], &attr);
        }
    }};
    while (uv_block_result < taskCount * 4) {
        usleep(1000);
    }
    EXPECT_EQ(uv_block_result, taskCount * 4);
    t1.join();
    t2.join();
}

HWTEST_F(DependencyTest, onsubmit_test, TestSize.Level0)
{
    ffrt_task_handle_t handle = nullptr;
    std::function<void()> cbOne = []() { printf("callback\n"); };
    ffrt_function_header_t* func = ffrt::create_function_wrapper(cbOne, ffrt_function_kind_general);
    ffrt::FFRTFacade::GetDMInstance().onSubmit(true, handle, func, nullptr, nullptr, nullptr);

    const std::vector<ffrt_dependence_t> wait_deps = {{ffrt_dependence_task, handle}};
    ffrt_deps_t wait{static_cast<uint32_t>(wait_deps.size()), wait_deps.data()};
    ffrt::FFRTFacade::GetDMInstance().onSubmit(true, handle, func, nullptr, &wait, nullptr);
    ffrt::FFRTFacade::GetDMInstance().onWait(&wait);

    EXPECT_NE(func, nullptr);
    ffrt_task_handle_destroy(handle);
}

/*
 * 测试用例名称：sample_nestedtask
 * 测试用例描述：提交数据依赖任务
 * 预置条件    ：无
 * 操作步骤    ：1、提交数据依赖任务
 * 预期结果    ：任务执行成功
 */
HWTEST_F(DependencyTest, sample_nestedtask, TestSize.Level0)
{
    int y = 0;
    ffrt::submit(
        [&]() {
            FFRT_LOGI("task 1");
            ffrt::submit([&]() { FFRT_LOGI("nested task 1.1"); }, {}, {});
            ffrt::submit([&]() { FFRT_LOGI("nested task 1.2"); }, {}, {});
            ffrt::wait();
            FFRT_LOGI("task 1 done");
        },
        {}, {});

    ffrt::submit(
        [&]() {
            FFRT_LOGI("task 2");
            ffrt::submit([&]() { FFRT_LOGI("nested task 2.1"); }, {}, {});
            ffrt::submit([&]() { FFRT_LOGI("nested task 2.2"); }, {}, {});
            ffrt::wait();
            y += 1;
        },
        {}, {});

    ffrt::wait();
    EXPECT_EQ(y, 1);
}

/*
 * 测试用例名称：sample_nestedtask_with_fake_deps
 * 测试用例描述：提交数据依赖任务,虚假数据依赖
 * 预置条件    ：无
 * 操作步骤    ：1、提交数据依赖任务
 * 预期结果    ：任务执行成功
 */
HWTEST_F(DependencyTest, sample_nestedtask_with_fake_deps, TestSize.Level0)
{
    int x; // 创建一个虚假的数据依赖，for test
    int y = 0;
    ffrt::submit(
        [&]() {
            FFRT_LOGI("task 1");
            ffrt::submit([&]() { FFRT_LOGI("nested task 1.1"); }, {}, {});
            ffrt::submit([&]() { FFRT_LOGI("nested task 1.2"); }, {}, {});
            ffrt::wait();
        },
        {}, {&x});

    ffrt::submit(
        [&]() {
            FFRT_LOGI("task 2");
            ffrt::submit([&]() { FFRT_LOGI("nested task 2.1"); }, {}, {});
            ffrt::submit([&]() { FFRT_LOGI("nested task 2.2"); }, {}, {});
            y += 1;
            ffrt::wait();
        },
        {&x}, {});

    ffrt::wait();
    EXPECT_EQ(y, 1);
}

/*
 * 测试用例名称：test_multithread_nested_case
 * 测试用例描述：提交嵌套数据依赖任务
 * 预置条件    ：无
 * 操作步骤    ：1、提交嵌套数据依赖任务
 * 预期结果    ：任务执行成功
 */
HWTEST_F(DependencyTest, test_multithread_nested_case, TestSize.Level0)
{
    auto func = []() {
        int x = 0;
        FFRT_LOGI("submit task 1");
        ffrt::submit(
            [&]() {
                ffrt::submit(
                    [&]() {
                        FFRT_LOGI("x = %d", x);
                        EXPECT_EQ(x, 0);
                    },
                    {&x}, {});
                ffrt::submit(
                    [&]() {
                        x++;
                        EXPECT_EQ(x, 1);
                    },
                    {&x}, {&x});
                ffrt::submit(
                    [&]() {
                        FFRT_LOGI("x = %d", x);
                        EXPECT_EQ(x, 1);
                    },
                    {&x}, {});
                ffrt::wait();
            },
            {&x}, {&x});
        FFRT_LOGI("submit task 2");
        ffrt::submit(
            [&]() {
                ffrt::submit(
                    [&]() {
                        FFRT_LOGI("x = %d", x);
                        EXPECT_EQ(x, 1);
                    },
                    {&x}, {});
                ffrt::submit(
                    [&]() {
                        x++;
                        EXPECT_EQ(x, 2);
                    },
                    {&x}, {&x});
                ffrt::submit(
                    [&]() {
                        FFRT_LOGI("x = %d", x);
                        EXPECT_EQ(x, 2);
                    },
                    {&x}, {});
                ffrt::wait();
            },
            {&x}, {&x});
        ffrt::wait();
    };

    std::vector<std::thread> ts;
    for (auto i = 0; i < 7; i++) {
        ts.emplace_back(func);
    }
    for (auto& t : ts) {
        t.join();
    }
}