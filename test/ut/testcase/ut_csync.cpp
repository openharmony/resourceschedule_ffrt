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
HWTEST_F(SyncTest, mutexattr_nullptr_fail, TestSize.Level0)
{
    int ret = ffrt_mutexattr_init(nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_settype(nullptr, 0);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_gettype(nullptr, nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_destroy(nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ffrt_mutexattr_t attr;
    int type = ffrt_mutex_default;
    ffrt_mutexattr_init(&attr);
    ret = ffrt_mutexattr_settype(&attr, ffrt_mutex_normal);
    ret = ffrt_mutexattr_settype(&attr, ffrt_mutex_default);
    ret = ffrt_mutexattr_settype(&attr, -1);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_gettype(&attr, nullptr);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt_mutexattr_gettype(&attr, &type);
    EXPECT_EQ(ret, ffrt_success);
    ffrt_mutexattr_destroy(&attr);
}

/**
 * @tc.name: mutex_nullptr_fail
 * @tc.desc: Test function of mutex when the input is nullptr;
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, mutex_nullptr_fail, TestSize.Level0)
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

    EXPECT_EQ(ffrt_mutex_lock_wait(nullptr), ffrt_error_inval);
    EXPECT_EQ(ffrt_mutex_unlock_wake(nullptr), ffrt_error_inval);
}

/**
 * @tc.name: mutex_try_lock
 * @tc.desc: Test function of mutex:try_lock
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, mutex_try_lock, TestSize.Level0)
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
HWTEST_F(SyncTest, recursive_mutex_try_lock, TestSize.Level0)
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
 * @tc.name: shared_mutex_try_lock
 * @tc.desc: Test function of shared mutex:try_lock
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, shared_mutex_try_lock, TestSize.Level0)
{
    ffrt::shared_mutex lock;
    lock.lock_shared();
    EXPECT_TRUE(lock.try_lock_shared());
    EXPECT_FALSE(lock.try_lock());
    lock.unlock_shared();
    lock.unlock_shared();

    lock.lock();
    EXPECT_FALSE(lock.try_lock_shared());
    EXPECT_FALSE(lock.try_lock());
    lock.unlock();

    EXPECT_TRUE(lock.try_lock());
    lock.unlock();
}

/**
 * @tc.name: mutex_lock_with_BlockThread
 * @tc.desc: Test function of mutex:lock in Thread mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, mutex_lock_with_BlockThread, TestSize.Level0)
{
    int x = 0;
    ffrt::mutex lock;
    ffrt::submit([&]() {
        ffrt_this_task_set_legacy_mode(true);
        ffrt::this_task::sleep_for(10ms);
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
 * @tc.name: set_legacy_mode_within_nested_task
 * @tc.desc: Test function of mutex:lock in Thread mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, set_legacy_mode_within_nested_task, TestSize.Level0)
{
    int x = 0;
    ffrt::submit([&]() {
        ffrt_this_task_set_legacy_mode(true);
        ffrt_this_task_set_legacy_mode(true);
        ffrt::CPUEUTask* ctx = static_cast<ffrt::CPUEUTask*>(ffrt::ExecuteCtx::Cur()->task);
        EXPECT_EQ(ctx->Block() == ffrt::BlockType::BLOCK_THREAD, true);
        EXPECT_EQ(ctx->GetBlockType() == ffrt::BlockType::BLOCK_THREAD, true);
        ctx->Wake();
        ffrt::submit([&]() {
            ffrt_this_task_set_legacy_mode(true);
            ffrt::CPUEUTask* ctx = static_cast<ffrt::CPUEUTask*>(ffrt::ExecuteCtx::Cur()->task);
            EXPECT_EQ(ctx->Block() == ffrt::BlockType::BLOCK_THREAD, true);
            EXPECT_EQ(ctx->GetBlockType() == ffrt::BlockType::BLOCK_THREAD, true);
            x++;
            EXPECT_EQ(x, 1);
            ffrt_this_task_set_legacy_mode(false);
            ffrt_this_task_set_legacy_mode(false);
            ctx = static_cast<ffrt::CPUEUTask*>(ffrt::ExecuteCtx::Cur()->task);
            int legacycount = ctx->legacyCountNum;
            EXPECT_EQ(legacycount, -1);
            }, {}, {});
        ffrt::wait();
        ffrt_this_task_set_legacy_mode(false);
        ffrt_this_task_set_legacy_mode(false);
        ctx = static_cast<ffrt::CPUEUTask*>(ffrt::ExecuteCtx::Cur()->task);
        int legacycount = ctx->legacyCountNum;
        EXPECT_EQ(legacycount, 0);
        auto expectedBlockType = ffrt::USE_COROUTINE? ffrt::BlockType::BLOCK_COROUTINE : ffrt::BlockType::BLOCK_THREAD;
        EXPECT_EQ(ctx->Block(), expectedBlockType);
        EXPECT_EQ(ctx->GetBlockType(), expectedBlockType);
        }, {}, {});
    ffrt::wait();
    EXPECT_EQ(x, 1);
}

HWTEST_F(SyncTest, class_data_align, TestSize.Level0)
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

HWTEST_F(SyncTest, lock_stress, TestSize.Level0)
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

HWTEST_F(SyncTest, lock_stress_c_api, TestSize.Level0)
{
    // trigger lazy init
    ffrt::submit([&]() {}, {}, {});
    ffrt::wait();

    const int N = 10;
    const int M = 1000;
    const int J = 10000;
    auto lock = std::make_unique<ffrt::mutex>();
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
}

/**
 * @tc.name: recursive_lock_stress
 * @tc.desc: Test C++ function of recursive mutex:lock in stress mode
 * @tc.type: FUNC
 */
HWTEST_F(SyncTest, recursive_lock_stress, TestSize.Level0)
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
HWTEST_F(SyncTest, recursive_lock_stress_c_api, TestSize.Level0)
{
    // trigger lazy init
    ffrt::submit([&]() {}, {}, {});
    ffrt::wait();

    const int N = 10;
    const int M = 1000;
    const int J = 10000;
    auto lock = std::make_unique<ffrt::recursive_mutex>();
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
}

void WaitforInThreadMode(std::function<void(std::atomic<bool>& ready)> waitFunc, std::function<void()> notifyFunc)
{
    std::atomic<bool> ready(false);
    std::thread th1([&waitFunc, &ready] {
        waitFunc(ready);
    });
    while (!ready.load()) {
        std::this_thread::yield();
    }
    std::thread th2(notifyFunc);
    th1.join();
    th2.join();
}

void WaitforInCoroutineMode(std::function<void(std::atomic<bool>& ready)> waitFunc, std::function<void()> notifyFunc)
{
    std::atomic<bool> ready(false);
    ffrt::submit([&waitFunc, &ready]() {
        waitFunc(ready);
    });
    while (!ready.load()) {
        std::this_thread::yield();
    }
    ffrt::submit(notifyFunc);
    ffrt::wait();
}

HWTEST_F(SyncTest, conditionTestWaitfor, TestSize.Level0)
{
    ffrt::mutex lock_;
    ffrt::condition_variable cond;
    std::atomic_bool flag {false};
    auto waitFunc = [&lock_, &cond, &flag] (std::atomic<bool>& ready) {
        std::unique_lock lck(lock_);
        bool ret = cond.wait_for(lck, std::chrono::nanoseconds::min(), [&flag] { return flag.load(); });
        EXPECT_FALSE(ret);

        auto start = std::chrono::high_resolution_clock::now();
        ret = cond.wait_for(lck, 100ms, [&flag] { return flag.load(); });
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        EXPECT_GE(elapsed, 100);

        ready.store(true);

        ret = cond.wait_for(lck, std::chrono::nanoseconds::max(), [&] { return flag.load(); });
        EXPECT_TRUE(ret);
        EXPECT_TRUE(flag.load());
    };
    auto notifyFunc = [&lock_, &cond, &flag] () {
        std::unique_lock lck(lock_);
        flag.store(true);
        cond.notify_all();
    };

    // Running test in Thread Mode
    WaitforInThreadMode(waitFunc, notifyFunc);
    flag.store(false);
    // Running test in Coroutine Mode
    WaitforInCoroutineMode(waitFunc, notifyFunc);
}

HWTEST_F(SyncTest, conditionTestDataRace, TestSize.Level0)
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

HWTEST_F(SyncTest, sharedMutexTestInit, TestSize.Level0)
{
    // init when attr is not nullptr,
    int x = 0;
    ffrt::shared_mutex smut;
    ffrt_rwlockattr_t attr;

    attr.storage = 1;
    x = ffrt_rwlock_init(&smut, &attr);
    EXPECT_EQ(x, ffrt_error_inval);
    x = ffrt_rwlock_init(nullptr, &attr);
    EXPECT_EQ(x, ffrt_error_inval);
}

/*
 * 测试用例名称：sharedMutexTest
 * 测试用例描述：legacy任务调用shared_mutex加解锁接口
 * 预置条件    ：无
 * 操作步骤    ：1、初始化shared_mutex
                 2、提交任务
                 3、调用shared_mutex加解锁接口
                 4、设置legacy模式后，调用shared_mutex加解锁接口
 * 预期结果    ：任务按预期执行
 */
HWTEST_F(SyncTest, mutexTest, TestSize.Level0)
{
    ffrt::mutex mut;

    int taskCount = 10;

    auto block = [&]() {
        mut.lock();
        usleep(10000);
        mut.unlock();
        ffrt_this_task_set_legacy_mode(true);
        mut.lock();
        usleep(10000);
        mut.unlock();
    };

    for (int i = 0; i < taskCount; i++) {
        ffrt::submit(block);
    }

    ffrt::wait();
}

HWTEST_F(SyncTest, thread1, TestSize.Level0)
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

HWTEST_F(SyncTest, thread2, TestSize.Level0)
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

HWTEST_F(SyncTest, thread_with_qos, TestSize.Level0)
{
    int a = 0;
    auto task = [&] {
        a++;
    };
    ffrt::thread(static_cast<int>(ffrt::qos_user_initiated), task).join();
    EXPECT_EQ(1, a);
}

HWTEST_F(SyncTest, thread_with_name, TestSize.Level0)
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

HWTEST_F(SyncTest, thread_with_ref_check, TestSize.Level0)
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

HWTEST_F(SyncTest, thread_with_ref, TestSize.Level0)
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
    EXPECT_TRUE(f1.valid());
    EXPECT_TRUE(f2.valid());
    f1.wait();
    f2.wait();
    f3.wait();
    std::cout << "Done!\nResults are: "
              << f1.get() << ' ' << f2.get() << ' ' << f3.get() << '\n';
    t.join();
}

HWTEST_F(SyncTest, future_wait_void, TestSize.Level1)
{
    vector<int> result(3, 0);
    ffrt::packaged_task<void()> task([&] { result[0] = 1; });
    ffrt::future<void> f1 = task.get_future();
    ffrt::thread t(std::move(task));
    ffrt::thread t1;
    t1 = std::move(t);

    ffrt::future<void> f2 = ffrt::async([&] { result[1] = 2; });

    ffrt::promise<void> p;
    ffrt::future<void> f3 = p.get_future();
    ffrt::promise<void> p1;
    ffrt::future<void> f4;
    p1 = std::move(p);
    f4 = std::move(f3);
    ffrt::thread([&] { result[2] = 3; p1.set_value(); }).detach();

    EXPECT_TRUE(f1.valid());
    EXPECT_TRUE(f2.valid());
    EXPECT_TRUE(f4.valid());
    f1.wait();
    f2.wait();
    f4.wait();
    f1.get();
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[1], 2);
    EXPECT_EQ(result[2], 3);
    t1.join();
}

/*
* 测试用例名称：ffrt_sleep_test
* 测试用例描述：测试ffrt_usleep接口
* 预置条件    ：无
* 操作步骤    ：1.提交两个普通任务，一个设置为legacy任务
                2.分别调用ffrt_usleep接口
* 预期结果    ：任务调用成功
*/
HWTEST_F(SyncTest, ffrt_sleep_test, TestSize.Level0)
{
    ffrt::submit([]() {
        ffrt_this_task_set_legacy_mode(true);
        ffrt::this_task::sleep_for(30ms);
    });

    ffrt::submit([]() {
        ffrt::this_task::sleep_for(30ms);
    });

    ffrt::wait();
}

/*
* 测试用例名称：ffrt_yield_test
* 测试用例描述：测试ffrt_yield接口
* 预置条件    ：无
* 操作步骤    ：1.提交两个普通任务，一个设置为legacy任务
                2.分别调用ffrt_yield接口
* 预期结果    ：任务调用成功
*/
HWTEST_F(SyncTest, ffrt_yield_test, TestSize.Level0)
{
    ffrt::submit([]() {
        ffrt_this_task_set_legacy_mode(true);
        ffrt::this_task::yield();
    });

    ffrt::submit([]() {
        ffrt::this_task::yield();
    });

    ffrt::wait();
}

HWTEST_F(SyncTest, rwlock_api_test, TestSize.Level0)
{
    int x = 0;
    x = ffrt_rwlock_wrlock(nullptr);
    EXPECT_EQ(x, ffrt_error_inval);
    x = ffrt_rwlock_trywrlock(nullptr);
    EXPECT_EQ(x, ffrt_error_inval);
    x = ffrt_rwlock_rdlock(nullptr);
    EXPECT_EQ(x, ffrt_error_inval);
    x = ffrt_rwlock_tryrdlock(nullptr);
    EXPECT_EQ(x, ffrt_error_inval);
    x = ffrt_rwlock_unlock(nullptr);
    EXPECT_EQ(x, ffrt_error_inval);
    x = ffrt_rwlock_destroy(nullptr);
    EXPECT_EQ(x, ffrt_error_inval);
}
