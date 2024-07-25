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

#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

#include <gtest/gtest.h>

#include "eu/rtg_ioctl.h"
#include "dfx/log/ffrt_log_api.h"

using namespace ffrt;
using namespace testing::ext;

class RTGTest : public testing::Test {
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

HWTEST_F(RTGTest, rtg_init_test, TestSize.Level1)
{
    bool enabled = RTGCtrl::Instance().Enabled();
    FFRT_LOGE("RTGCtrl Init %s", enabled ? "Success" : "Failed");
}

HWTEST_F(RTGTest, rtg_get_group_test, TestSize.Level1)
{
    int tgid = RTGCtrl::Instance().GetThreadGroup();
    if (tgid < 0) {
        FFRT_LOGE("Failed to Get RTG id %d", tgid);
    }

    bool ret = RTGCtrl::Instance().PutThreadGroup(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Put RTG id %d", tgid);
    }
}

HWTEST_F(RTGTest, rtg_set_window_size_test, TestSize.Level1)
{
    constexpr int WINDOW_SIZE = 10000;

    int tgid = RTGCtrl::Instance().GetThreadGroup();
    if (tgid < 0) {
        FFRT_LOGE("Failed to Get RTG id %d", tgid);
    }

    bool ret = RTGCtrl::Instance().SetGroupWindowSize(tgid, WINDOW_SIZE);
    if (!ret) {
        FFRT_LOGE("Failed to Set Window Size %d", WINDOW_SIZE);
    }

    ret = RTGCtrl::Instance().PutThreadGroup(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Put RTG id %d", tgid);
    }
}

HWTEST_F(RTGTest, rtg_set_invalid_interval_test, TestSize.Level1)
{
    constexpr int INVALID_INTERVAL = 10;

    int tgid = RTGCtrl::Instance().GetThreadGroup();
    if (tgid < 0) {
        FFRT_LOGE("Failed to Get RTG id %d", tgid);
    }

    bool ret = RTGCtrl::Instance().SetInvalidInterval(tgid, INVALID_INTERVAL);
    if (!ret) {
        FFRT_LOGE("Failed to Set Invalid Interval %d", INVALID_INTERVAL);
    }

    ret = RTGCtrl::Instance().PutThreadGroup(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Put RTG id %d", tgid);
    }
}

HWTEST_F(RTGTest, rtg_set_preferred_cluster_test, TestSize.Level1)
{
    constexpr int CLUSTER_ID = 0;

    int tgid = RTGCtrl::Instance().GetThreadGroup();
    if (tgid < 0) {
        FFRT_LOGE("Failed to Get RTG id %d", tgid);
    }

    bool ret = RTGCtrl::Instance().SetPreferredCluster(tgid, CLUSTER_ID);
    if (!ret) {
        FFRT_LOGE("Failed to Set Preferred Cluster %d", CLUSTER_ID);
    }

    ret = RTGCtrl::Instance().PutThreadGroup(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Put RTG id %d", tgid);
    }
}

HWTEST_F(RTGTest, rtg_begin_end_test, TestSize.Level1)
{
    int tgid = RTGCtrl::Instance().GetThreadGroup();
    if (tgid < 0) {
        FFRT_LOGE("Failed to Get RTG id %d", tgid);
    }

    bool ret = RTGCtrl::Instance().Begin(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Begin");
    }

    ret = RTGCtrl::Instance().End(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to End");
    }

    ret = RTGCtrl::Instance().PutThreadGroup(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Put RTG id %d", tgid);
    }
}

HWTEST_F(RTGTest, rtg_add_tread_test, TestSize.Level1)
{
    constexpr int THREAD_NUM = 8;
    bool ret;
    int tgid = RTGCtrl::Instance().GetThreadGroup();
    if (tgid < 0) {
        FFRT_LOGE("Failed to Get RTG id %d", tgid);
    }

    std::vector<std::thread> threads;

    std::mutex tidMutex;
    std::vector<pid_t> tids;

    std::mutex condMutex;
    std::condition_variable cond;

    auto f = [&]() {
        pid_t tid = RTGCtrl::GetTID();

        {
            std::unique_lock lock(tidMutex);
            tids.emplace_back(tid);
        }

        std::unique_lock lock(condMutex);
        cond.wait(lock);
    };

    for (int i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back(std::thread(f));
    }

    while (true) {
        std::unique_lock lock(tidMutex);
        if (tids.size() >= THREAD_NUM) {
            break;
        }
    }

    for (auto tid : tids) {
        ret = RTGCtrl::Instance().JoinThread(tgid, tid);
        if (!ret) {
            FFRT_LOGE("Failed To Join Thread %d", tid);
        }
    }

    ret = RTGCtrl::Instance().UpdatePerfFreq(tgid, 960000);
    for (auto tid : tids) {
        auto [t_load, t_runtime] = RTGCtrl::Instance().UpdateAndGetLoad(tgid, tid);
        FFRT_LOGE("Get Load %lu runtime %lu", t_load, t_runtime);
        ret = RTGCtrl::Instance().RemoveThread(tgid, tid);
        if (!ret) {
            FFRT_LOGE("Failed To Leave Thread %d", tid);
        }
    }

    cond.notify_all();
    for (auto& thread : threads) {
        thread.join();
    }

    ret = RTGCtrl::Instance().PutThreadGroup(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Put RTG id %d", tgid);
    }
}

HWTEST_F(RTGTest, rtg_update_util_test, TestSize.Level1)
{
    constexpr int THREAD_NUM = 8;

    int tgid = RTGCtrl::Instance().GetThreadGroup();
    if (tgid < 0) {
        FFRT_LOGE("Failed to Get RTG id %d", tgid);
    }

    std::vector<std::thread> threads;

    std::mutex tidMutex;
    std::vector<pid_t> tids;

    std::mutex condMutex;
    std::condition_variable cond;

    auto f = [&]() {
        pid_t tid = RTGCtrl::GetTID();

        {
            std::unique_lock lock(tidMutex);
            tids.emplace_back(tid);
        }

        std::unique_lock lock(condMutex);
        cond.wait(lock);
    };

    for (int i = 0; i < THREAD_NUM; ++i) {
        threads.emplace_back(std::thread(f));
    }

    while (true) {
        std::unique_lock lock(tidMutex);
        if (tids.size() >= THREAD_NUM) {
            break;
        }
    }

    for (auto tid : tids) {
        bool ret = RTGCtrl::Instance().JoinThread(tgid, tid);
        if (!ret) {
            FFRT_LOGE("Failed To Join Thread %d", tid);
        }
    }

    bool ret = RTGCtrl::Instance().Begin(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Begin");
    }

    for (int util = 8; util <= 1024; util <<= 1) {
        auto [load, runtime] = RTGCtrl::Instance().UpdateAndGetLoad(tgid);
        FFRT_LOGE("Get Load %lu runtime %lu", load, runtime);

        ret = RTGCtrl::Instance().UpdatePerfUtil(tgid, util);
        if (!ret) {
            FFRT_LOGE("Failed To Update Util %d", util);
        }
    }

    for (auto tid : tids) {
        ret = RTGCtrl::Instance().RemoveThread(tgid, tid);
        if (!ret) {
            FFRT_LOGE("Failed To Leave Thread %d", tid);
        }
    }

    ret = RTGCtrl::Instance().End(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to End");
    }

    cond.notify_all();
    for (auto& thread : threads) {
        thread.join();
    }

    ret = RTGCtrl::Instance().PutThreadGroup(tgid);
    if (!ret) {
        FFRT_LOGE("Failed to Put RTG id %d", tgid);
    }
}
