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
#include "dfx/log/ffrt_log_api.h"
#include "dfx/bbox/bbox.h"
#include "tm/cpu_task.h"
#include "tm/io_task.h"
#include "tm/queue_task.h"
#include "tm/scpu_task.h"
#include "tm/task_factory.h"
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

#if defined(__clang__)
#define OPTIMIZE_OFF __attribute__((optnone))
#elif defined(__GNUC__)
#define OPTIMIZE_OFF __attribute__((optimize(0)))
#else
#define OPTIMIZE_OFF
#endif

namespace {
void OPTIMIZE_OFF OnePlusForTest(void* data)
{
    *(int*)data += 1;
}
} // namespace

HWTEST_F(CoreTest, task_ctx_success_01, TestSize.Level0)
{
    auto func1 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    task_attr_private attr;
    attr.qos_ = qos_user_interactive;
    auto task1 = std::make_unique<SCPUEUTask>(&attr, nullptr, 0);
    auto func2 = ([]() {std::cout << std::endl << " push a task " << std::endl;});
    auto task2 = std::make_unique<SCPUEUTask>(nullptr, task1.get(), 0);
    QoS qos = QoS(static_cast<int>(qos_inherit));
    task2->SetQos(qos);
    EXPECT_EQ(task2->qos_, static_cast<int>(qos_user_interactive));
}

/**
 * @tc.name: TaskBlockTypeCheck
 * @tc.desc: Test function of TaskBase::Block and TaskBase::Wake
 * @tc.type: FUNC
 */
HWTEST_F(CoreTest, TaskBlockTypeCheck, TestSize.Level0)
{
    auto task = std::make_unique<SCPUEUTask>(nullptr, nullptr, 0);

    // when executing task is root
    EXPECT_EQ(task->GetBlockType(), BlockType::BLOCK_THREAD);

    // when executing task is nullptr
    EXPECT_EQ(task->Block(), BlockType::BLOCK_THREAD);

    auto parent = std::make_unique<SCPUEUTask>(nullptr, nullptr, 0);
    task->parent = parent.get();

    auto expectedBlockType = USE_COROUTINE? BlockType::BLOCK_COROUTINE :  BlockType::BLOCK_THREAD;
    // when task is not in legacy mode
    EXPECT_EQ(task->Block(), expectedBlockType);
    EXPECT_EQ(task->GetBlockType(), expectedBlockType);
    task->Wake();

    // when task is in legacy mode
    task->legacyCountNum++;
    EXPECT_EQ(task->Block(), BlockType::BLOCK_THREAD);
    EXPECT_EQ(task->GetBlockType(), BlockType::BLOCK_THREAD);
    task->Wake();

    // when task's legacy mode canceled
    task->legacyCountNum--;
    EXPECT_EQ(task->Block(), expectedBlockType);
    EXPECT_EQ(task->GetBlockType(), expectedBlockType);
}

/**
 * 测试用例名称：task_attr_set_timeout
 * 测试用例描述：验证task_attr的设置timeout接口
 * 预置条件：创建有效的task_attr
 * 操作步骤：设置timeout值，通过get接口与设置值对比
 * 预期结果：设置成功
 */
HWTEST_F(CoreTest, task_attr_set_timeout, TestSize.Level0)
{
    ffrt_task_attr_t* attr = (ffrt_task_attr_t *) malloc(sizeof(ffrt_task_attr_t));
    ffrt_task_attr_init(attr);
    ffrt_task_attr_set_timeout(attr, 1000);
    uint64_t timeout = ffrt_task_attr_get_timeout(attr);
    EXPECT_EQ(timeout, 1000);
    ffrt_task_attr_set_timeout(attr, UINT64_MAX); // 测试时间溢出截断功能
    uint64_t maxUsCount = 1000000ULL * 100 * 60 * 60 * 24 * 365; // 100 year
    EXPECT_EQ(ffrt_task_attr_get_timeout(attr), maxUsCount);
    free(attr);
}

/**
 * 测试用例名称：task_attr_set_timeout_nullptr
 * 测试用例描述：验证task_attr的设置timeout接口的异常场景
 * 预置条件：针对nullptr进行设置
 * 操作步骤：设置timeout值，通过get接口与设置值对比
 * 预期结果：设置失败，返回值为0
 */
HWTEST_F(CoreTest, task_attr_set_timeout_nullptr, TestSize.Level0)
{
    ffrt_task_attr_t* attr = nullptr;
    ffrt_task_attr_set_timeout(attr, 1000);
    uint64_t timeout = ffrt_task_attr_get_timeout(attr);
    EXPECT_EQ(timeout, 0);
}

/**
 * 测试用例名称：task_attr_set_stack_size
 * 测试用例描述：验证task_attr的设置stack_size接口
 * 预置条件：创建有效的task_attr
 * 操作步骤：设置stack_size值，通过get接口与设置值对比
 * 预期结果：设置成功
 */
HWTEST_F(CoreTest, task_attr_set_stack_size, TestSize.Level0)
{
    ffrt_task_attr_t* attr = (ffrt_task_attr_t *) malloc(sizeof(ffrt_task_attr_t));
    ffrt_task_attr_init(attr);
    ffrt_task_attr_set_stack_size(attr, 1024 * 1024);
    uint64_t stackSize = ffrt_task_attr_get_stack_size(attr);
    EXPECT_EQ(stackSize, 1024 * 1024);
    free(attr);
}

/**
 * 测试用例名称：ffrt_task_handle_ref_nullptr
 * 测试用例描述：验证task_handle的增加、消减引用计数接口的异常场景
 * 预置条件：针对nullptr进行设置
 * 操作步骤：对nullptr进行调用
 * 预期结果：接口校验异常场景成功，用例正常执行结束
 */
HWTEST_F(CoreTest, ffrt_task_handle_ref_nullptr, TestSize.Level0)
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
HWTEST_F(CoreTest, ffrt_task_handle_ref, TestSize.Level0)
{
    // 验证notify_worker的功能
    int result = 0;
    ffrt_task_attr_t taskAttr;
    (void)ffrt_task_attr_init(&taskAttr); // 初始化task属性，必须
    ffrt_task_attr_set_delay(&taskAttr, 10000); // 延时10ms执行
    std::function<void()>&& OnePlusFunc = [&result]() { result += 1; };
    ffrt_task_handle_t handle = ffrt_submit_h_base(ffrt::create_function_wrapper(OnePlusFunc), {}, {}, &taskAttr);
    EXPECT_GT(ffrt_task_handle_get_id(handle), 0);
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
HWTEST_F(CoreTest, WaitFailWhenReuseHandle, TestSize.Level0)
{
    int i = 0;
    std::vector<ffrt::dependence> deps;
    {
        auto h = ffrt::submit_h([&i] { printf("task0 done\n"); i++;});
        printf("task0 handle: %p\n:", static_cast<void*>(h));
        ffrt::dependence d(h);
        ffrt::dependence dep = d;
        deps.emplace_back(dep);
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
HWTEST_F(CoreTest, ffrt_task_get_tid_test, TestSize.Level0)
{
    auto task = std::make_unique<ffrt::SCPUEUTask>(nullptr, nullptr, 0);
    auto queueTask = std::make_unique<ffrt::QueueTask>(nullptr);
    pthread_t tid = ffrt_task_get_tid(task.get());
    EXPECT_EQ(tid, 0);

    tid = ffrt_task_get_tid(queueTask.get());
    EXPECT_EQ(tid, 0);

    tid = ffrt_task_get_tid(nullptr);
    EXPECT_EQ(tid, 0);
}

/*
* 测试用例名称：ffrt_get_cur_cached_task_id_test
* 测试用例描述：测试ffrt_get_cur_cached_task_id接口
* 预置条件    ：设置ExecuteCtx::Cur->lastGid_为自定义值
* 操作步骤    ：调用ffrt_get_cur_cached_task_id接口
* 预期结果    ：ffrt_get_cur_cached_task_id返回值与自定义值相同
*/
HWTEST_F(CoreTest, ffrt_get_cur_cached_task_id_test, TestSize.Level0)
{
    auto ctx = ffrt::ExecuteCtx::Cur();
    ctx->lastGid_ = 15;
    EXPECT_EQ(ffrt_get_cur_cached_task_id(), 15);

    ffrt::submit([] {});
    ffrt::wait();

    EXPECT_NE(ffrt_get_cur_cached_task_id(), 0);
}

/*
* 测试用例名称：ffrt_skip_task_test
* 测试用例描述：测试ffrt_skip接口
* 预置条件    ：无
* 操作步骤    ：1.提交普通延时执行的任务，不notifyworker，并获取句柄
               2.调用ffrt_skip接口，入参为句柄
* 预期结果    ：任务取消成功
*/
HWTEST_F(CoreTest, ffrt_skip_task_test, TestSize.Level0)
{
    auto h = ffrt::submit_h([]() {}, {}, {}, ffrt::task_attr().delay(10000)); // 10ms
    int cancel_ret = ffrt::skip(h);
    EXPECT_EQ(cancel_ret, 0);
    ffrt::wait();
}

/*
* 测试用例名称：ffrt_get_cur_task_test
* 测试用例描述：测试ffrt_get_cur_task接口
* 预置条件    ：提交ffrt任务
* 操作步骤    ：1.在ffrt任务中调用ffrt_get_cur_task接口
               2.在非ffrt任务中调用ffrt_get_cur_task接口
* 预期结果    ：1.返回的task地址不为空
               2.返回的task地址不为空
*/
HWTEST_F(CoreTest, ffrt_get_cur_task_test, TestSize.Level0)
{
    void* taskPtr = nullptr;
    ffrt::submit([&] {
        taskPtr = ffrt_get_cur_task();
    });
    ffrt::wait();

    EXPECT_NE(taskPtr, nullptr);
    taskPtr = ffrt_get_cur_task();
    EXPECT_EQ(taskPtr, nullptr);
}

/*
* 测试用例名称：ffrt_this_task_get_qos_test
* 测试用例描述：测试ffrt_this_task_get_qos接口
* 预置条件    ：提交qos=3的ffrt任务
* 操作步骤    ：在ffrt任务中调用ffrt_this_task_get_qos接口
* 预期结果    ：ffrt_this_task_get_qos返回值=3
*/
HWTEST_F(CoreTest, ffrt_this_task_get_qos_test, TestSize.Level0)
{
    ffrt_qos_t qos = 0;
    ffrt::submit([&] {
        qos = ffrt_this_task_get_qos();
    }, ffrt::task_attr().qos(ffrt_qos_user_initiated));
    ffrt::wait();
    EXPECT_EQ(qos, ffrt::QoS(ffrt_qos_user_initiated)());
}

/*
* 测试用例名称：ffrt_set_sched_mode
* 测试用例描述：ffrt_set_sched_mode EU调度模式设置
* 预置条件    ：NA
* 操作步骤    ：在非ffrt任务中调用ffrt_set_sched_mode接口
* 预期结果    ：设置EU调度策略为默认模式、性能模式或节能模式
*/
HWTEST_F(CoreTest, ffrt_set_sched_mode, TestSize.Level0)
{
    ffrt::sched_mode_type sched_type = ffrt::ExecuteUnit::Instance().GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_default_mode));

    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_energy_saving_mode);
    sched_type = ffrt::ExecuteUnit::Instance().GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_energy_saving_mode));

    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_performance_mode);
    sched_type = ffrt::ExecuteUnit::Instance().GetSchedMode(ffrt::QoS(ffrt::qos_default));
    EXPECT_EQ(static_cast<int>(sched_type), static_cast<int>(ffrt::sched_mode_type::sched_performance_mode));
    ffrt_set_sched_mode(ffrt::QoS(ffrt::qos_default), ffrt_sched_default_mode);
}

/*
* 测试用例名称：ffrt_set_worker_stack_size
* 测试用例描述：ffrt_set_worker_stack_size 设置worker线程栈大小
* 预置条件    ：NA
* 操作步骤    ：在非ffrt任务中调用ffrt_set_worker_stack_size接口
* 预期结果    ：能够处理正常和异常stackSize和qos的值
*/
HWTEST_F(CoreTest, ffrt_set_worker_stack_size, TestSize.Level0)
{
    ffrt_error_t ret;
    int qosMin = ffrt_qos_background;
    int qosMax = ffrt::QoS::Max();
    int stackSizeMin = PTHREAD_STACK_MIN;
    // 设置异常Qos
    ret = ffrt::set_worker_stack_size(qosMin - 1, stackSizeMin);
    EXPECT_EQ(ret, ffrt_error_inval);
    ret = ffrt::set_worker_stack_size(qosMax, stackSizeMin);
    EXPECT_EQ(ret, ffrt_error_inval);

    // 设置异常stacksize
    ret = ffrt::set_worker_stack_size(qosMin, stackSizeMin - 1);
    EXPECT_EQ(ret, ffrt_error_inval);
}

namespace ffrt {
template <typename T>
TaskFactory<T>& TaskFactory<T>::Instance()
{
    static TaskFactory<T> fac;
    return fac;
}
}

namespace TmTest {
class MyTask : public ffrt::TaskBase {
public:
    MyTask() : ffrt::TaskBase(ffrt_invalid_task, nullptr) {}
private:
    void FreeMem() override { ffrt::TaskFactory<MyTask>::Free(this); }
    void Prepare() override {}
    void Ready() override {}
    void Pop() override {}
    void Cancel() override {}
    void Finish() override {}
    void Execute() override {}
    ffrt::BlockType Block() override { return ffrt::BlockType::BLOCK_THREAD; }
    void Wake() override {}
    void SetQos(const QoS& newQos) override {}
    std::string GetLabel() const override { return "my-task"; }
    ffrt::BlockType GetBlockType() const override { return ffrt::BlockType::BLOCK_THREAD; }
};

void TestTaskFactory(bool isSimpleAllocator)
{
    MyTask* task = ffrt::TaskFactory<MyTask>::Alloc();
    uint32_t taskCount = ffrt::TaskFactory<MyTask>::GetUnfreedMem().size();
    EXPECT_EQ(taskCount, ffrt::TaskFactory<MyTask>::GetUnfreedMemSize());
#ifdef FFRT_BBOX_ENABLE
    EXPECT_FALSE(ffrt::TaskFactory<MyTask>::HasBeenFreed(task));
#else
    EXPECT_TRUE(isSimpleAllocator || !ffrt::TaskFactory<MyTask>::HasBeenFreed(task));
#endif

    std::vector<ffrt::TaskBase*> tasks;
    new(task) MyTask();
    tasks.push_back(task);
    for (auto task : tasks) {
        task->DecDeleteRef();
    }

    uint32_t newCount = ffrt::TaskFactory<MyTask>::GetUnfreedMem().size();
    EXPECT_EQ(newCount, ffrt::TaskFactory<MyTask>::GetUnfreedMemSize());
#ifdef FFRT_BBOX_ENABLE
    EXPECT_GT(taskCount, newCount);
#else
    if (!isSimpleAllocator) {
        EXPECT_GT(taskCount, ffrt::TaskFactory<MyTask>::GetUnfreedMem().size());
    }
#endif
    EXPECT_TRUE(ffrt::TaskFactory<MyTask>::HasBeenFreed(task));
}

template <typename T>
class CustomTaskManager {
public:
    T* Alloc()
    {
        mutex.lock();
        T* res = reinterpret_cast<T*>(malloc(sizeof(T)));
        allocedTask.insert(res);
        mutex.unlock();
        return res;
    }

    void Free(T* task)
    {
        mutex.lock();
        allocedTask.erase(task);
        free(task);
        mutex.unlock();
    }

    void LockMem()
    {
        mutex.lock();
    }

    void UnlockMem()
    {
        mutex.unlock();
    }

    bool HasBeenFreed(T* task)
    {
        return allocedTask.find(task) == allocedTask.end();
    }

    std::vector<void*> GetUnfreedMem()
    {
        std::vector<void*> res;
        res.reserve(allocedTask.size());
        std::transform(allocedTask.begin(), allocedTask.end(), std::inserter(res, res.end()), [](T* ptr) {
            return static_cast<void*>(ptr);
        });
        return res;
    }

    std::size_t GetUnfreedMemSize()
    {
        return allocedTask.size();
    }

private:
    std::mutex mutex;
    std::set<T*> allocedTask;
};
} // namespace TmTest

/*
* 测试用例名称：ffrt_task_factory_test_001
* 测试用例描述：测试使用SimpleAllocator时，任务能够成功申请释放
* 预置条件    ：注册使用SimpleAllocator的内存分配函数
* 操作步骤    ：使用TaskFactory申请一个自定义Task的实例，初始化后调用各自的DecDeleteRef释放
* 预期结果    ：能够正常申请和释放，释放前后GetUnFreedMem读取数组的值小于释放前
*/
HWTEST_F(CoreTest, ffrt_task_factory_test_001, TestSize.Level0)
{
    ffrt::TaskFactory<TmTest::MyTask>::RegistCb(
        ffrt::SimpleAllocator<TmTest::MyTask>::AllocMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::FreeMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::FreeMem_,
        ffrt::SimpleAllocator<TmTest::MyTask>::getUnfreedMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::getUnfreedMemSize,
        ffrt::SimpleAllocator<TmTest::MyTask>::HasBeenFreed,
        ffrt::SimpleAllocator<TmTest::MyTask>::LockMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::UnlockMem);

    TmTest::TestTaskFactory(true);
}

/*
* 测试用例名称：ffrt_task_factory_test_002
* 测试用例描述：测试使用自定义管理器时，任务能够成功申请释放
* 预置条件    ：注册自定义Task内存分配函数
* 操作步骤    ：使用TaskFactory申请一个自定义Task的实例，初始化后调用各自的DecDeleteRef释放
* 预期结果    ：能够正常申请和释放，释放前后GetUnFreedMem读取数组的值小于释放前
*/
HWTEST_F(CoreTest, ffrt_task_factory_test_002, TestSize.Level0)
{
    TmTest::CustomTaskManager<TmTest::MyTask> custom_manager;
    ffrt::TaskFactory<TmTest::MyTask>::RegistCb(
        [&] () -> TmTest::MyTask* { return custom_manager.Alloc(); },
        [&] (TmTest::MyTask* task) { custom_manager.Free(task); },
        [&] (TmTest::MyTask* task) { custom_manager.Free(task); },
        [&] () -> std::vector<void*> { return custom_manager.GetUnfreedMem(); },
        [&] () -> std::size_t { return custom_manager.GetUnfreedMemSize(); },
        [&] (TmTest::MyTask* task) { return custom_manager.HasBeenFreed(task); },
        [&] () { custom_manager.LockMem(); },
        [&] () { custom_manager.UnlockMem(); });

    TmTest::TestTaskFactory(false);
}

HWTEST_F(CoreTest, ffrt_submit_h_f, TestSize.Level0)
{
    ffrt_task_attr_t attr;
    (void)ffrt_task_attr_init(&attr);

    int result = 0;
    ffrt_task_handle_t task = ffrt_submit_h_f(OnePlusForTest, &result, nullptr, nullptr, &attr);
    const std::vector<ffrt_dependence_t> wait_deps = {{ffrt_dependence_task, task}};
    ffrt_deps_t wait{static_cast<uint32_t>(wait_deps.size()), wait_deps.data()};
    ffrt_wait_deps(&wait);
    ffrt_task_handle_destroy(task);

    EXPECT_EQ(result, 1);
}

/*
* 测试用例名称：ffrt_task_factory_test_003
* 测试用例描述：测试使用SimpleAllocator时，UVTask的TaskFactory接口正常
* 预置条件    ：无
* 操作步骤    ：1.向TaskFactory申请一个UVTask实例
               2.调用TaskFactory的HasBeenFreed、Free_接口
* 预期结果    ：任务是否释放符合预期
*/
HWTEST_F(CoreTest, ffrt_task_factory_test_003, TestSize.Level0)
{
    ffrt::UVTask* task = ffrt::TaskFactory<ffrt::UVTask>::Alloc();
    EXPECT_EQ(ffrt::TaskFactory<ffrt::UVTask>::HasBeenFreed(task), false);
    ffrt::TaskFactory<ffrt::UVTask>::Free_(task);
    EXPECT_EQ(ffrt::TaskFactory<ffrt::UVTask>::HasBeenFreed(task), true);
}

HWTEST_F(CoreTest, ffrt_submit_f, TestSize.Level0)
{
    ffrt_task_attr_t attr;
    (void)ffrt_task_attr_init(&attr);

    int result = 0;
    ffrt_submit_f(OnePlusForTest, &result, nullptr, nullptr, &attr);
    ffrt::wait();
    EXPECT_EQ(result, 1);
}

HWTEST_F(CoreTest, ffrt_core_test, TestSize.Level0)
{
    std::function<void()> cbOne = []() { printf("callback\n"); };
    ffrt_function_header_t* func = ffrt::create_function_wrapper(cbOne, ffrt_function_kind_general);
    uint64_t size = 1 * 1024 * 1024;
    ffrt_task_attr_t task_attr;
    (void)ffrt_task_attr_init(&task_attr);
    ffrt_task_handle_t handle = ffrt_submit_h_base(func, {}, {}, &task_attr);
    ffrt_task_handle_get_id(handle);

    ffrt_task_attr_init(nullptr);
    ffrt_task_attr_destroy(nullptr);
    ffrt_task_attr_set_name(&task_attr, nullptr);
    ffrt_task_attr_get_name(&task_attr);
    const char * name = ffrt_task_attr_get_name(nullptr);
    EXPECT_EQ(name, nullptr);
    ffrt_task_attr_set_stack_size(&task_attr, size);
    ffrt_task_attr_set_stack_size(nullptr, size);
    uint64_t id = ffrt_task_handle_get_id(nullptr);
    EXPECT_EQ(id, 0);
    ffrt::wait({handle});
    ffrt_task_handle_destroy(handle);
}

/*
 * 测试用例名称: ffrt_task_factory_deleteRef_test
 * 测试用例描述: 测试使用自定义管理器时，在并发情况下增减引用计数接口正常
 * 预置条件    : 注册自定义Task内存分配函数
 * 操作步骤    : 创建2批线程，分别使用自定义管理器创建任务并减少引用计数和获取尝试增加引用计数成功的任务并减少引用计数
 * 预期结果    : 任务引用计数符合预期
*/
HWTEST_F(CoreTest, ffrt_task_factory_deleteRef_test, TestSize.Level0)
{
    ffrt::TaskFactory<TmTest::MyTask>::RegistCb(
        ffrt::SimpleAllocator<TmTest::MyTask>::AllocMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::FreeMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::FreeMem_,
        ffrt::SimpleAllocator<TmTest::MyTask>::getUnfreedMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::getUnfreedMemSize,
        ffrt::SimpleAllocator<TmTest::MyTask>::HasBeenFreed,
        ffrt::SimpleAllocator<TmTest::MyTask>::LockMem,
        ffrt::SimpleAllocator<TmTest::MyTask>::UnlockMem);

    int threadCnt = 100;
    std::thread decDeleteRefThreads[threadCnt];
    std::thread incDeleteRefThreads[threadCnt];
    for (int threadIndex = 0; threadIndex < threadCnt; threadIndex++) {
        decDeleteRefThreads[threadIndex] = std::thread([&] {
            std::vector<TmTest::MyTask*> tasks;
            int taskCount = 1000;
            for (int i = 0; i < taskCount; i++) {
                TmTest::MyTask* task = ffrt::TaskFactory<TmTest::MyTask>::Alloc();
                new(task) TmTest::MyTask();
                tasks.push_back(task);
            }
            for (auto& task : tasks) {
                EXPECT_TRUE(task->DecDeleteRef() > 0);
            }
        });

        incDeleteRefThreads[threadIndex] = std::thread([&] {
            std::vector<void*> unfreeVec = TaskFactory<TmTest::MyTask>::GetUnfreedTasksFiltered();
            for (auto& unfree : unfreeVec) {
                auto t = reinterpret_cast<TmTest::MyTask*>(unfree);
                EXPECT_TRUE(t->DecDeleteRef() > 0);
            }
        });
    }

    for (auto& t : decDeleteRefThreads) {
        t.join();
    }
    for (auto& t : incDeleteRefThreads) {
        t.join();
    }
}
