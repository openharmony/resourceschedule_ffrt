#include <thread>
#include <chrono>
#include <gtest/gtest.h>
#include "ffrt_inner.h"
#include "c/loop.h"

using namespace std;
using namespace ffrt;
using namespace testing;

class LoopTest : public testing::Test {
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

    virtual void TeatDown()
    {
    }
};

void* ThreadFunc(void* p)
{
    int ret = ffrt_loop_run(p);
    EXPECT_EQ(ret, 0);
    return nullptr;
}

/*
 * 测试用例名称：loop_null_queue_create_fail
 * 测试用例描述：非法队列创建loop失败
 * 预置条件    ：无
 * 操作步骤    ：1、创建loop失败
 * 
 * 预期结果    ：创建失败
 */
TEST_F(LoopTest, loop_null_queue_create_fail)
{
    auto loop = ffrt_loop_create(nullptr);
    EXPECT_EQ(loop, nullptr);
}

/*
 * 测试用例名称：loop_serial_queue_create_succ
 * 测试用例描述：serial队列创建loop失败
 * 预置条件    ：1、调用串行队列创建接口创建serial队列
 * 操作步骤    ：1、创建loop
 * 
 * 预期结果    ：创建失败
 */
TEST_F(LoopTest, loop_serial_queue_create_succ)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_serial, "test_queue", &queue_attr);

    auto loop = ffrt_loop_create(queue_handle);
    EXPECT_EQ(loop, nullptr);

    ffrt_queue_attr_destroy(&queue_attr);
    ffrt_queue_destotry(queue_handle);
}

/*
 * 测试用例名称：loop_concurrent_queue_create_succ
 * 测试用例描述：无任务concurrent队列创建loop成功
 * 预置条件    ：1、调用串行队列创建接口创建concurrent队列
 * 操作步骤    ：1、创建loop
 * 
 * 预期结果    ：创建成功
 */
TEST_F(LoopTest, loop_concurrent_queue_create_succ)
{
    ffrt_queue_attr_t queue_attr;
    (void)ffrt_queue_attr_init(&queue_attr);
    ffrt_queue_t queue_handle = ffrt_queue_create(ffrt_queue_concurrent, "test_queue", &queue_attr);

    auto loop = 
}