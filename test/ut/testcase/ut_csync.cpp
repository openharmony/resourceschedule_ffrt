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

#include <mutex>
#include <future>
#include <chrono>
#include <random>
#include <gtest/gtest.h>
#include "sync/sync.h"
#include "ffrt_inner.h"
#include "dfx/log/ffrt_log_api.h"
#include "c/thread.h"
#include "c/ffrt_ipc.h"
#include "tm/cpu_task.h"
#include "../common.h"

extern "C" int ffrt_mutexattr_init(ffrt_mutexattr_t* attr);
extern "C" int ffrt_mutexattr_settype(ffrt_mutexattr_t* attr, int type);
extern "C" int ffrt_mutexattr_gettype(ffrt_mutexattr_t* attr, int* type);
extern "C" int ffrt_mutexattr_destroy(ffrt_mutexattr_t* attr);
extern "C" int ffrt_mutex_init(ffrt_mutex_t *mutex, const ffrt_mutexattr_t* attr);
extern "C" int ffrt_mutex_lock(ffrt_mutex_t *mutex);
extern "C" int ffrt_mutex_unlock(ffrt_mutex_t *mutex);
extern "C" int ffrt_mutex_trylock(ffrt_mutex_t *mutex);
extern "C" int ffrt_mutex_destroy(ffrt_mutex_t *mutex);

using namespace std;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class SyncTest : public testing::Test {
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

/**
 * @tc.name: mutexattr_nullptr_fail
 * @tc.desc: Test function of mutexattr when the input is nullptr;
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, mutexattr_nullptr_fail, TestSize.Level1)
{
    int ret = ffrt_mutexattr_init(nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_settype(nullptr, 0);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_gettype(nullptr, nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_destroy(nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
}

/**
 * @tc.name: mutex_nullptr_fail
 * @tc.desc: Test function of mutex when the input is nullptr;
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, mutex_nullptr_fail, TestSize.Level1)
{
    int ret = ffrt_mutex_init(nullptr, nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutex_lock(nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutex_unlock(nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutex_trylock(nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ffrt_mutex_destroy(nullptr);
}

/**
 * @tc.name: mutex_try_lock
 * @tc.desc: Test function of mutex:try_lock
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, mutex_try_lock, TestSize.Level1)
{
    int val = -1;
    ffrt::mutex lock;
    lock.lock();
    val = lock.try_lock();
    EXPECT_EQ(val, 0);
    lock.unlock();
    val = lock.try_lock();
    EXPECT_EQ(val, 1);
    lock.unlock();
    lock.unlock();
}

/**
 * @tc.name: recursive_mutex_try_lock
 * @tc.desc: Test function of recursive mutex:try_lock
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, recursive_mutex_try_lock, TestSize.Level1)
{
    int val = -1;
    ffrt::recursive_mutex lock;
    lock.lock();
    val = lock.try_lock();
    EXPECT_EQ(val, 1);
    lock.unlock();
    val = lock.try_lock();
    EXPECT_EQ(val, 1);
    lock.unlock();
    lock.unlock();
}

/**
 * @tc.name: mutex_lock_with_BlockThread
 * @tc.desc: Test function of mutex:lock in Thread mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, mutex_lock_with_BlockThread, TestSize.Level1)
{
    int x = 0;
    ffrt::mutex lock;
    ffrt::submit([&]() {
        ffrt::this_task::sleep_for(10ms);
        ffrt_this_task_set_legacy_mode(true);
        lock.lock();
        ffrt::submit([&]() {
            EXPECT_EQ(x, 1);
            }, {&x}, {});
        ffrt::submit([&]() {
            x++;
            EXPECT_EQ(x, 2);
            }, {&x}, {&x});
        ffrt::submit([&]() {
            EXPECT_EQ(x, 2);
            }, {&x}, {});
        ffrt::wait();
        lock.unlock();
        ffrt_this_task_set_legacy_mode(false);
        }, {}, {}, ffrt::task_attr().name("t2"));

    ffrt::submit([&]() {
        ffrt_this_task_set_legacy_mode(true);
        lock.lock();
        ffrt::submit([&]() {
            EXPECT_EQ(x, 0);
            }, {&x}, {});
        ffrt::submit([&]() {
            x++;
            EXPECT_EQ(x, 1);
            }, {&x}, {&x});
        ffrt::submit([&]() {
            EXPECT_EQ(x, 1);
            }, {&x}, {});
        ffrt::wait();
        lock.unlock();
        ffrt_this_task_set_legacy_mode(false);
        }, {}, {}, ffrt::task_attr().name("t1"));
    ffrt::wait();
}

/**
 * @tc.name: shared_mutex_lock_with_BlockThread
 * @tc.desc: Test function of shared mutex:lock in Thread mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, shared_mutex_lock_with_BlockThread, TestSize.Level1)
{
    int x = 0;
    const int N = 10;
    ffrt::shared_mutex lock;
    for (int i = 0; i < N; ++i) {
        ffrt::submit([&]() {
            ffrt_this_task_set_legacy_mode(true);
            lock.lock();
            ffrt::this_task::sleep_for(10ms);
            x++;
            lock.unlock();
            ffrt_this_task_set_legacy_mode(false);
            }, {}, {}, ffrt::task_attr().name("t1"));
    }
    ffrt::submit([&]() {
        ffrt_this_task_set_legacy_mode(true);
        lock.lock();
        printf("x is %d", x);
        lock.unlock();
        ffrt_this_task_set_legacy_mode(false);
        }, {}, {}, ffrt::task_attr().name("t2"));
    
    ffrt::wait();
    EXPECT_EQ(x, N);
}

/**
 * @tc.name: set_legacy_mode_within_nested_task
 * @tc.desc: Test function of mutex:lock in Thread mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, set_legacy_mode_within_nested_task, TestSize.Level1)
{
    int x = 0;
    ffrt::submit([&]() {
        ffrt_this_task_set_legacy_mode(true);
        ffrt_this_task_set_legacy_mode(true);
        ffrt::CPUEUTask* ctx = ffrt::ExecuteCtx::Cur()->task;
        bool result = ffrt::LegacyMode(ctx);
        EXPECT_EQ(result, 1);
        ffrt::submit([&]() {
            ffrt_this_task_set_legacy_mode(true);
            ffrt::CPUEUTask* ctx = ffrt::ExecuteCtx::Cur()->task;
            bool result = ffrt::LegacyMode(ctx);
            EXPECT_EQ(result, 1);
            x++;
            EXPECT_EQ(x, 1);
            ffrt_this_task_set_legacy_mode(false);
            ffrt_this_task_set_legacy_mode(false);
            ctx = ffrt::ExecuteCtx::Cur()->task;
            int legacycount = ctx->legacyCountNum;
            EXPECT_EQ(legacycount, -1);
            }, {}, {});
        ffrt::wait();
        ffrt_this_task_set_legacy_mode(false);
        ffrt_this_task_set_legacy_mode(false);
        ctx = ffrt::ExecuteCtx::Cur()->task;
        int legacycount = ctx->legacyCountNum;
        EXPECT_EQ(legacycount, 0);
        }, {}, {});
    ffrt::wait();
    EXPECT_EQ(x, 1);
}

HWTEST_F(SyncTest, class_data_align, TestSize.Level1)
{
    struct memTest {
        bool isFlag; // Construct an unaligned address
        ffrt::mutex mtx;
        ffrt::task_attr taskAttr;
        ffrt::task_handle taskHandle;
        ffrt::condition_variable cv;
        ffrt::thread t;
    };
    memTest m;
    {
        ffrt::mutex* mtxAddr = &m.mtx;
        uintptr_t addr_int = reinterpret_cast<uintptr_t>(mtxAddr);
        EXPECT_EQ((addr_int % 4), 0);

        ffrt::task_attr* attrAddr = &m.taskAttr;
        addr_int = reinterpret_cast<uintptr_t>(attrAddr);
        EXPECT_EQ((addr_int % 4), 0);

        ffrt::task_handle* handleAddr = &m.taskHandle;
        addr_int = reinterpret_cast<uintptr_t>(handleAddr);
        EXPECT_EQ((addr_int % 4), 0);

        ffrt::condition_variable* cvAddr = &m.cv;
        addr_int = reinterpret_cast<uintptr_t>(cvAddr);
        EXPECT_EQ((addr_int % 4), 0);

        ffrt::thread* tAddr = &m.t;
        addr_int = reinterpret_cast<uintptr_t>(tAddr);
        EXPECT_EQ((addr_int % 4), 0);
    }
}

HWTEST_F(SyncTest, lock_stress, TestSize.Level1)
{
    // trigger lazy init
    ffrt::submit([&]() {}, {}, {});
    ffrt::wait();

    const int N = 10;
    const int M = 1000;
    const int J = 10000;
    ffrt::mutex lock;
    // std::mutex lock;
    int acc = 0;
    for (int i = 0; i < N; ++i) {
        ffrt::submit(
            [&]() {
                for (int j = 0; j < M; ++j) {
                    lock.lock();
                    acc++;
                    lock.unlock();
                }
            },
            {}, {});
    }

    for (int j = 0; j < J; ++j) {
        lock.lock();
        acc++;
        lock.unlock();
    }

    ffrt::wait();
    EXPECT_EQ(acc, (M * N + J));
}

HWTEST_F(SyncTest, lock_stress_c_api, TestSize.Level1)
{
    // trigger lazy init
    ffrt::submit([&]() {}, {}, {});
    ffrt::wait();

    const int N = 10;
    const int M = 1000;
    const int J = 10000;
    ffrt::mutex* lock = new ffrt::mutex;
    int acc = 0;
    for (int i = 0; i < N; ++i) {
        ffrt::submit(
            [&]() {
                for (int j = 0; j < M; ++j) {
                    lock->lock();
                    acc++;
                    lock->unlock();
                }
            },
            {}, {});
    }

    for (int j = 0; j < J; ++j) {
        lock->lock();
        acc++;
        lock->unlock();
    }

    ffrt::wait();
    EXPECT_EQ(acc, (M * N + J));
    delete lock;
}

/**
 * @tc.name: recursive_lock_stress
 * @tc.desc: Test C++ function of recursive mutex:lock in stress mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, recursive_lock_stress, TestSize.Level1)
{
    // trigger lazy init
    ffrt::submit([&]() {}, {}, {});
    ffrt::wait();

    const int N = 10;
    const int M = 1000;
    const int J = 10000;
    ffrt::recursive_mutex lock;
    // std::mutex lock;
    int acc = 0;
    for (int i = 0; i < N; ++i) {
        ffrt::submit(
            [&]() {
                for (int j = 0; j < M; ++j) {
                    lock.lock();
                    acc++;
                    lock.unlock();
                }
            },
            {}, {});
    }

    for (int j = 0; j < J; ++j) {
        lock.lock();
        acc++;
        lock.unlock();
    }

    ffrt::wait();
    EXPECT_EQ(acc, (M * N + J));
}

/**
 * @tc.name: recursive_lock_stress
 * @tc.desc: Test C function of recursive mutex:lock in stress mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, recursive_lock_stress_c_api, TestSize.Level1)
{
    // trigger lazy init
    ffrt::submit([&]() {}, {}, {});
    ffrt::wait();

    const int N = 10;
    const int M = 1000;
    const int J = 10000;
    ffrt::recursive_mutex* lock = new ffrt::recursive_mutex;
    int acc = 0;
    for (int i = 0; i < N; ++i) {
        ffrt::submit(
            [&]() {
                for (int j = 0; j < M; ++j) {
                    lock->lock();
                    acc++;
                    lock->unlock();
                }
            },
            {}, {});
    }

    for (int j = 0; j < J; ++j) {
        lock->lock();
        acc++;
        lock->unlock();
    }

    ffrt::wait();
    EXPECT_EQ(acc, (M * N + J));
    delete lock;
}

HWTEST_F(SyncTest, conditionTestNotifyOne, TestSize.Level1)
{
    ffrt::condition_variable cond;
    int a = 0;
    ffrt::mutex lock_;

    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            cond.wait(lck, [&] { return a == 1; });
        },
        {}, {});

    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            a = 1;
            cond.notify_one();
        },
        {}, {});
    ffrt::wait();
}

HWTEST_F(SyncTest, conditionTestNotifyAll, TestSize.Level1)
{
    ffrt::condition_variable cond;
    int a = 0;
    ffrt::mutex lock_;

    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            cond.wait(lck, [&] { return a == 1; });
        },
        {}, {});
    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            cond.wait(lck, [&] { return a == 1; });
        },
        {}, {});
    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            cond.wait(lck, [&] { return a == 1; });
        },
        {}, {});

    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            a = 1;
            cond.notify_all();
        },
        {}, {});
    ffrt::wait();
}

HWTEST_F(SyncTest, conditionTestWaitfor, TestSize.Level1)
{
    ffrt::condition_variable cond;
    std::atomic_int a = 0;
    ffrt::mutex lock_;

    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            cond.wait_for(lck, 100ms, [&] { return a == 1; });
            EXPECT_EQ(a, 2);
        },
        {}, {});
    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            cond.wait_for(lck, 150ms, [&] { return a == 2; });
            EXPECT_EQ(a, 2);
        },
        {}, {});
    ffrt::submit(
        [&]() {
            std::unique_lock lck(lock_);
            a = 2;
            cond.notify_all();
        },
        {}, {});
    ffrt::wait();
}

HWTEST_F(SyncTest, conditionTestDataRace, TestSize.Level1)
{
    std::atomic_bool exit {false};
    ffrt::mutex mtx;
    ffrt::condition_variable cv;
    std::atomic_bool start {false};

    ffrt::thread th {[&] {
        while (!exit) {
            if (start) {
                cv.notify_one();
                ffrt::this_task::sleep_for(1us);
            }
        }
    }};

    start = true;
    for (int i = 0; i < 2000; ++i) {
        std::unique_lock lk(mtx);
        cv.wait_for(lk, 1us);
    }
    exit = true;
    th.join();
    exit = false;
    start = true;
    ffrt::thread th1 {[&] {
        for (int i = 0; i < 2000; ++i) {
            std::unique_lock lk(mtx);
            cv.wait_for(lk, 1us);
        }
        exit = true;
    }};

    while (!exit) {
        if (start) {
            cv.notify_one();
            ffrt::this_task::sleep_for(1us);
        }
    }

    th1.join();
}

static void NotifyOneTest(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    FFRT_LOGE("[RUN ] notifyone");
    int value = 0;
    bool flag {false};
    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            cv.wait(lk, [&] { return flag; });
            EXPECT_TRUE(lk.owns_lock());
            value = 123;
        },
        {}, {});

    EXPECT_EQ(value, 0);

    ffrt::submit(
        [&]() {
            {
                std::unique_lock lk(mtx);
                flag = true;
            }
            cv.notify_one();
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

static void WaitUntilTimeoutTest(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    constexpr auto eps = 3ms;

    FFRT_LOGE("[RUN ] WaitUntil timeout&notifyone");
    int value = 0;
    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            EXPECT_EQ(static_cast<int>(cv.wait_until(lk, std::chrono::steady_clock::now() + 30ms)),
                static_cast<int>(ffrt::cv_status::timeout));
            EXPECT_TRUE(lk.owns_lock());
            value = 123;
        },
        {}, {});

    EXPECT_EQ(value, 0);

    ffrt::submit(
        [&]() {
            ffrt::this_task::sleep_for(30ms + eps);
            cv.notify_one();
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

static void WaitUtilFlagTest_1(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    constexpr auto eps = 3ms;

    FFRT_LOGE("[RUN ] WaitUntil flag&notifyone");
    int value = 0;
    bool flag {false};

    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            EXPECT_TRUE(!cv.wait_until(lk, std::chrono::steady_clock::now() + 30ms, [&] { return flag; }));
            value = 123;
        },
        {}, {});

    EXPECT_EQ(value, 0);

    ffrt::submit(
        [&]() {
            ffrt::this_task::sleep_for(30ms + eps);
            cv.notify_one();
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

static void WaitUtilFlagTest_2(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    int value = 0;
    bool flag {false};

    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            EXPECT_TRUE(cv.wait_until(lk, std::chrono::steady_clock::now() + 30ms, [&] { return flag; }));
            value = 123;
        },
        {}, {});

    EXPECT_EQ(value, 0);

    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            flag = true;
            cv.notify_one();
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

static void WaitForTest_1(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    constexpr auto eps = 3ms;

    int value = 0;
    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            EXPECT_EQ(static_cast<int>(cv.wait_for(lk, 30ms)), static_cast<int>(ffrt::cv_status::timeout));
            EXPECT_TRUE(lk.owns_lock());
            value = 123;
        },
        {}, {});

    EXPECT_EQ(value, 0);

    ffrt::submit(
        [&]() {
            ffrt::this_task::sleep_for(30ms + eps);
            cv.notify_one();
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

static void WaitForTest_2(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    int value = 0;
    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            ffrt::submit(
                [&]() {
                    std::unique_lock lk(mtx);
                    cv.notify_one();
                },
                {}, {});
            EXPECT_EQ(static_cast<int>(cv.wait_for(lk, 30ms)), static_cast<int>(ffrt::cv_status::no_timeout));
            EXPECT_EQ(value, 0);
            EXPECT_TRUE(lk.owns_lock());
            value = 123;
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

static void WaitForTest_3(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    constexpr auto eps = 3ms;

    int value = 0;
    bool flag {false};

    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            EXPECT_TRUE(!cv.wait_for(lk, 30ms, [&] { return flag; }));
            value = 123;
        },
        {}, {});

    EXPECT_EQ(value, 0);

    ffrt::submit(
        [&]() {
            ffrt::this_task::sleep_for(30ms + eps);
            cv.notify_one();
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

static void WaitForTest_4(ffrt::mutex& mtx, ffrt::condition_variable& cv)
{
    int value = 0;
    bool flag {false};

    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            EXPECT_TRUE(cv.wait_for(lk, 30ms, [&] { return flag; }));
            value = 123;
        },
        {}, {});

    EXPECT_EQ(value, 0);

    ffrt::submit(
        [&]() {
            std::unique_lock lk(mtx);
            flag = true;
            cv.notify_one();
        },
        {}, {});

    ffrt::wait();

    EXPECT_EQ(value, 123);
}

HWTEST_F(SyncTest, conditionTest, TestSize.Level1)
{
    ffrt::mutex mtx;
    ffrt::condition_variable cv;

    NotifyOneTest(mtx, cv);
    WaitUntilTimeoutTest(mtx, cv);
    WaitUtilFlagTest_1(mtx, cv);
    WaitUtilFlagTest_2(mtx, cv);
    WaitForTest_1(mtx, cv);
    WaitForTest_2(mtx, cv);
    WaitForTest_3(mtx, cv);
    WaitForTest_4(mtx, cv);
}

static void LockTest(ffrt::shared_mutex& smtx)
{
    int x = 0;
    const int N = 100;
    const int R = 200;

    ffrt::submit(
        [&]() {
            for (int i = 0; i < N; i++) {
                smtx.lock();
                x++;
                smtx.unlock();
            }
        },
        {}, {});

    for (int j = 0; j < N; ++j) {
        smtx.lock();
        x++;
        smtx.unlock();
    }

    ffrt::wait();
    EXPECT_EQ(x, R);
}

static void TryLockTest(ffrt::shared_mutex& smtx)
{
    int x = 0;
    const int N = 100;
    ffrt::submit(
        [&]() {
            smtx.lock();
            ffrt::this_task::sleep_for(20ms);
            smtx.unlock();
        },
        {}, {});

    ffrt::this_task::sleep_for(2ms);

    bool ret = smtx.try_lock();
    EXPECT_EQ(ret, false);
    if (ret) {
        smtx.unlock();
    }
    ffrt::wait();

    ret = smtx.try_lock();
    EXPECT_EQ(ret, true);
    if (ret) {
        smtx.unlock();
    }
}

static void LockSharedTest(ffrt::shared_mutex& smtx)
{
    int x = 0;
    const int N = 100;

    ffrt::submit(
        [&]() {
            smtx.lock_shared();
            ffrt::this_task::sleep_for(20ms);
            x = N;
            smtx.unlock_shared();
        },
        {}, {});
    ffrt::this_task::sleep_for(2ms);

    smtx.lock_shared();
    EXPECT_EQ(x, 0);
    smtx.unlock_shared();

    smtx.lock();
    EXPECT_EQ(x, N);
    smtx.unlock();

    ffrt::wait();

    smtx.lock_shared();
    EXPECT_EQ(x, N);
    smtx.unlock_shared();
}

static void TryLockSharedTest(ffrt::shared_mutex& smtx)
{
    int x = 0;
    const int N = 100;

    ffrt::submit(
        [&]() {
            smtx.lock_shared();
            ffrt::this_task::sleep_for(20ms);
            x = N;
            smtx.unlock_shared();
        },
        {}, {});
    ffrt::this_task::sleep_for(2ms);

    bool ret = smtx.try_lock_shared();
    EXPECT_EQ(ret, true);
    EXPECT_EQ(x, 0);
    if (ret) {
        smtx.unlock_shared();
    }
    ffrt::wait();

    ret = smtx.try_lock_shared();
    EXPECT_EQ(ret, true);
    EXPECT_EQ(x, N);
    if (ret) {
        smtx.unlock_shared();
    }

    ffrt::submit(
        [&]() {
            smtx.lock();
            ffrt::this_task::sleep_for(20ms);
            x = 0;
            smtx.unlock();
        },
        {}, {});
    ffrt::this_task::sleep_for(2ms);

    ret = smtx.try_lock_shared();
    EXPECT_EQ(ret, false);
    EXPECT_EQ(x, N);
    if (ret) {
        smtx.unlock_shared();
    }
    ffrt::wait();

    ret = smtx.try_lock_shared();
    EXPECT_EQ(ret, true);
    EXPECT_EQ(x, 0);
    if (ret) {
        smtx.unlock_shared();
    }
}

HWTEST_F(SyncTest, sharedMutexTest, TestSize.Level1)
{
    ffrt::shared_mutex smtx;
    LockTest(smtx);
    TryLockTest(smtx);
    LockSharedTest(smtx);
    TryLockSharedTest(smtx);
}

HWTEST_F(SyncTest, thread1, TestSize.Level1)
{
    auto ThreadFunc1 = [](int a, const int& b) {
        FFRT_LOGW("a = %d, b = %d", a, b);
        return 0;
    };

    auto ThreadFunc2 = [](const char* a, const char* b) {
        FFRT_LOGW("%s %s", a, b);
        return 0;
    };

    {
        int value = 0;
        std::thread th0 {[&value, &ThreadFunc1, &ThreadFunc2] {
            std::thread th1 {ThreadFunc1, 10, 20};
            std::thread th2 {ThreadFunc2, "hello", "ffrt"};
            th1.join();
            th2.join();

            value = 123;
            FFRT_LOGW("value = %d", value);
        }};
        th0.join();
        assert(!th0.joinable());
        EXPECT_EQ(value, 123);
    }
    {
        int value = 0;
        ffrt::thread th0 {[&value, &ThreadFunc1, &ThreadFunc2] {
            ffrt::thread th1 {ThreadFunc1, 10, 20};
            ffrt::thread th2 {ThreadFunc2, "hello", "ffrt"};
            th1.join();
            th2.join();

            value = 123;
            FFRT_LOGW("value = %d", value);
        }};
        th0.join();
        assert(!th0.joinable());
        EXPECT_EQ(value, 123);
    }
}

void f1(int n)
{
    for (int i = 0; i < 5; ++i) {
        std::cout << "Thread 1 executing\n";
        ++n;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void f2(int& n)
{
    for (int i = 0; i < 5; ++i) {
        std::cout << "Thread 2 executing\n";
        ++n;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

class foo {
public:
    void bar()
    {
        for (int i = 0; i < 5; ++i) {
            std::cout << "Thread 3 executing\n";
            ++n;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    int n = 0;
};

class baz {
public:
    void operator()()
    {
        for (int i = 0; i < 5; ++i) {
            std::cout << "Thread 4 executing\n";
            ++n;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    int n = 0;
};

HWTEST_F(SyncTest, thread2, TestSize.Level1)
{
    {
        int n = 0;
        foo f;
        baz b;
        {
            std::thread t2(f1, n + 1);
            t2.detach(); // test detach
        }
        std::thread t1; // t1 is not a thread
        std::thread t2(f1, n + 1); // pass by value
        std::thread t3(f2, std::ref(n)); // pass by reference
        std::thread t4(std::move(t3)); // t4 is now running f2(). t3 is no longer a thread
        std::thread t5(&foo::bar, &f); // t5 runs foo::bar() on object f
        std::thread t6(b); // t6 runs baz::operator() on a copy of object b
        EXPECT_EQ(t1.joinable(), false);
        EXPECT_EQ(t2.joinable(), true);
        t2.join();
        EXPECT_EQ(t2.joinable(), false);
        t4.join();
        t5.join();
        t6.join();
        EXPECT_EQ(n, 5);
        EXPECT_EQ(f.n, 5);
        EXPECT_EQ(b.n, 0);
    }
    FFRT_LOGW("ffrt version");
    {
        int n = 0;
        foo f;
        baz b;
        {
            ffrt::thread t2(f1, n + 1);
            t2.detach(); // test detach
        }
        ffrt::thread t1; // t1 is not a thread
        ffrt::thread t2(f1, n + 1); // pass by value
        ffrt::thread t3(f2, std::ref(n)); // pass by reference
        ffrt::thread t4(std::move(t3)); // t4 is now running f2(). t3 is no longer a thread
        ffrt::thread t5(&foo::bar, &f); // t5 runs foo::bar() on object f
        ffrt::thread t6(b); // t6 runs baz::operator() on a copy of object b
        EXPECT_EQ(t1.joinable(), false);
        EXPECT_EQ(t2.joinable(), true);
        t2.join();
        EXPECT_EQ(t2.joinable(), false);
        EXPECT_EQ(t3.joinable(), false);
        EXPECT_EQ(t4.joinable(), true);
        t4.join();
        EXPECT_EQ(t4.joinable(), false);
        t5.join();
        t6.join();
        EXPECT_EQ(n, 5);
        EXPECT_EQ(f.n, 5);
        EXPECT_EQ(b.n, 0);
    }
}

HWTEST_F(SyncTest, thread_with_qos, TestSize.Level1)
{
    int a = 0;
    auto task = [&] {
        a++;
    };
    ffrt::thread(static_cast<int>(ffrt::qos_user_initiated), task).join();
    EXPECT_EQ(1, a);
}

HWTEST_F(SyncTest, thread_with_name, TestSize.Level1)
{
    int a = 0;
    auto task = [&] {
        a++;
    };
    std::string name = "thread_test";
    ffrt::thread(name.c_str(), static_cast<int>(ffrt::qos_user_initiated), task).join();
    EXPECT_EQ(1, a);
}

struct F {
    template<typename T, typename U>
    void operator()(T, U, int& a)
    {
        using std::is_same;
        using std::reference_wrapper;
        static_assert(is_same<T, reference_wrapper<int>>::value, "");
        static_assert(is_same<U, reference_wrapper<const int>>::value, "");
        a++;
    }
};

HWTEST_F(SyncTest, thread_with_ref_check, TestSize.Level1)
{
    int a = 0;
    ffrt::thread t(F{}, std::ref(a), std::cref(a), std::ref(a));
    t.join();
    EXPECT_EQ(1, a);
}

struct A {
    A() = default;
    explicit A(const A&) = default;
};

void func(const A&) { }

HWTEST_F(SyncTest, thread_with_ref, TestSize.Level1)
{
    ffrt::thread t(func, A{});
    t.join();
}

HWTEST_F(SyncTest, future_wait, TestSize.Level1)
{
    ffrt::packaged_task<int()> task([] { return 7; });
    ffrt::future<int> f1 = task.get_future();
    ffrt::thread t(std::move(task));

    ffrt::future<int> f2 = ffrt::async([] { return 8; });

    ffrt::promise<int> p;
    ffrt::future<int> f3 = p.get_future();
    ffrt::thread([&p] { p.set_value(9); }).detach();

    std::cout << "Waiting..." << std::flush;
    f1.wait();
    f2.wait();
    f3.wait();
    std::cout << "Done!\nResults are: "
              << f1.get() << ' ' << f2.get() << ' ' << f3.get() << '\n';
    t.join();
}
