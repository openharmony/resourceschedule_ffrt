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

#include<thread>
#include<chrono>
#include<gtest/gtest.h>
#include"ffrt.h"

using namespace std;
using namespace ffrt;
using namespace testing;

class CoroutineTest : public testing::Test {
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

    virtual void TeadDown()
    {
    }
};

const int BLOCKED_COUNT = 3;

typedef struct {
    int count;
}StacklessCoroutine1;

ffrt_coroutine_ret_t stackless_coroutine(void *co)
{
    ((StacklessCoroutine1*)(co))->count++;

    if (((StacklessCoroutine1*)(co))->count < BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    }else if (((StacklessCoroutine1*)(co))->count == BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    }else {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_ready;
    }
    return ffrt_coroutine_pending;
}

ffrt_coroutine_ret_t exec_stackless_coroutine(void *co)
{
    return stackless_coroutine(co);
}

void destroy_stackless_coroutine(void *co)
{
}

TEST_F(CoroutineTest, coroutine_submit_succ)
{
    StacklessCoroutine1 co1={0};
    StacklessCoroutine1 co2={0};
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_name(&attr, "stackless_coroutine");
    ffrt_task_attr_set_coroutine_type(&attr, ffrt_coroutine_stackless);
    int coroutine_type_=ffrt_task_attr_get_coroutine_type(&attr);
    ffrt_submit_coroutine((void *)co1, exec_stackless_coroutine, destroy_stackless_coroutine, NULL, NULL, &attr);
    ffrt_task_handle_t task1=ffrt_submit_h_coroutine((void *)co2, exec_stackless_coroutine,destroy_stackless_coroutine, \
        NULL, NULL, &attr);
    ffrt_wait();
    ffrt_task_handle_destroy(task1);
    EXPECT_EQ(coroutine_type_, 0);
    EXPECT_EQ(co1.count, 4);
    EXPECT_EQ(co2.count, 4);
}

TEST_F(CoroutineTest, coroutine_submit_fail)
{
    ffrt_task_get();
    ffrt_task_handle_destroy(nullptr);

    StacklessCoroutine1 co1 = {0};
    StacklessCoroutine1 co2 = {0};
    StacklessCoroutine1 co3 = {0};
    StacklessCoroutine1 co4 = {0};
    StacklessCoroutine1 co5 = {0};
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_name(&attr, "stackless_coroutine");
    ffrt_task_attr_set_coroutine_type(&attr, ffrt_coroutine_stackless);

    ffrt_submit_coroutine(nullptr, nullptr, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_t task1=ffrt_submit_h_coroutine(nullptr, nullptr, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_destroy(task1);

    ffrt_submit_coroutine((void *)&co1, nullptr, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_t task2=ffrt_submit_h_coroutine((void *)&co2, nullptr, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_destroy(task2);

    ffrt_submit_coroutine((void *)&co3, exec_stackless_coroutine, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_t task3=ffrt_submit_h_coroutine((void *)&co4, exec_stackless_coroutine, nullptr, NULL, NULL, &attr);
    ffrt_task_handle_destroy(task3);

    ffrt_task_attr_t attr_stackfull;
    ffrt_task_attr_init(&attr_stackfull);
    ffrt_task_attr_set_name(&attr_stackfull, "stackfull_coroutine");
    ffrt_task_attr_set_coroutine_type(&attr_stackfull, ffrt_coroutine_stackfull);
    ffrt_submit_coroutine((void *)&co5, nullptr, nullptr, NULL, NULL, &attr_stackfull);
    ffrt_task_handle_t task_stackfull=ffrt_submit_h_coroutine((void *)&co6, nullptr, nullptr, NULL, NULL, &attr_stackfull);
    ffrt_task_attr_destroy(task_stackfull);
}

StacklessCoroutine1 g_col = {0};
struct Waker
{
    void *phandle;
    void *handle;
}waker;

void wake_stackless_coroutine(void *arg)
{
    ffrt_wake_coroutine(((Waker *)arg)->phandle);
}

void destroy_wake_of_stackless_coroutine(void *arg)
{
    ffrt_task_handle_destroy(((Waker *)arg)->handle);
}

ffrt_coroutine_ret_t maintask_stackless_coroutine(void *co)
{
    ((StacklessCoroutine1*)(co))->count++;

    if (((StacklessCoroutine1*)(co))->count < BLOCKED_COUNT) {
        if (((StacklessCoroutine1*)(co))->count == 1) {
            ffrt_task_attr_t attr;
            ffrt_task_attr_init(&attr);
            ffrt_task_attr_set_name(&attr, "stackless_coroutine");
            ffrt_task_attr_set_coroutine_type(&attr, ffrt_coroutine_stackless);
            ffrt_set_wake_flag(true);
            ffrt_task_handle_t h=ffrt_submit_h_coroutine((void *)&g_col, exec_stackless_coroutine, destroy_stackless_coroutine, 
                NULL, NULL, &attr);
            waker.phandle=ffrt_task_get();
            waker.handle=h;
            ffrt_wake_by_handle(&waker, wake_stackless_coroutine, destroy_wake_of_stackless_coroutine, h);
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_pending;
        }
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    }else if (((StacklessCoroutine1*)(co))->count == BLOCKED_COUNT) {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_pending;
    }else {
        ffrt_wake_coroutine(ffrt_task_get());
        return ffrt_coroutine_ready;
    }
    return ffrt_coroutine_pending;
}

ffrt_coroutine_ret_t maintask_exec_stackless_coroutine(void *co)
{
    return maintask_stackless_coroutine(co);
}

void maintask_destroy_stackless_coroutine(void *co)
{
}

TEST_F(CoroutineTest, coroutine_wake_by_handle_succ)
{
    StacklessCoroutine1 co1 = {0};
    ffrt_task_attr_t maintask_attr;
    ffrt_task_attr_init(&maintask_attrr);
    ffrt_task_attr_set_name(&maintask_attr, "stackless_coroutine_maintask");
    ffrt_task_attr_set_coroutine_type(&maintask_attr, ffrt_coroutine_stackless);
    ffrt_task_handle_t maintask=ffrt_submit_h_coroutine((void *)&co1, maintask_exec_stackless_coroutine, \
        maintask_destroy_stackless_coroutine, NULL, NULL, &maintask_attr);
    ffrt_wait();
    ffrt_task_handle_destroy(maintask);
    EXPECT_EQ(co1.count, 4);
}

ffrt_coroutine_ret_t maintask_stackless_coroutine_fail(void *co)
{
    ((StacklessCoroutine1*)(co))->count++;

    if (((StacklessCoroutine1*)(co))->count < BLOCKED_COUNT) {
            if (((StacklessCoroutine1*)(co))->count == 1) {  
                ffrt_task_attr_t attr;
                ffrt_task_attr_init(&attr);
                ffrt_task_attr_set_name(&attr, "stackless_coroutine");
                ffrt_task_attr_set_coroutine_type(&attr, ffrt_coroutine_stackless);
                ffrt_task_handle_t h=ffrt_submit_h_coroutine((void *)&g_col, exec_stackless_coroutine, \
                    destroy_stackless_coroutine,NULL,NULL,&attr);
                waker.phandle=ffrt_task_get();
                waker.handle=h;

                ffrt_wake_by_handle(nullptr, nullptr, nullptr, nullptr);
                ffrt_wake_by_handle(&waker, nullptr, nullptr, h);
                ffrt_wake_by_handle(nullptr, nullptr, nullptr, nullptr);
                ffrt_wake_by_handle(&waker, nullptr, nullptr, h);
                ffrt_wake_by_handle(&waker, wake_stackless_coroutine, nullptr, h);
                ffrt_wake_by_handle(&waker, wake_stackless_coroutine, destroy_wake_of_stackless_coroutine, h);
                ffrt_wake_coroutine(ffrt_task_get());
                return ffrt_coroutine_pending;
            }
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_pending;
        }else if (((StacklessCoroutine1*)(co))->count == BLOCKED_COUNT) {
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_pending;
        }else {
            ffrt_wake_coroutine(ffrt_task_get());
            return ffrt_coroutine_ready;
        }
        return ffrt_coroutine_pending;
}

ffrt_coroutine_ret_t maintask_exec_stackless_coroutine_fail(void *co)
{
    return maintask_stackless_coroutine_fail(co);
}

void maintask_destroy_stackless_coroutine_fail(void *co)
{
}

TEST_F(CoroutineTest, coroutine_wake_by_handle_fail)
{
    StacklessCoroutine1 co1 = {0};
    ffrt_task_attr_t maintask_attr;
    ffrt_task_attr_init(&maintask_attrr);
    ffrt_task_attr_set_name(&maintask_attr, "stackless_coroutine_maintask");
    ffrt_task_attr_set_coroutine_type(&maintask_attr, ffrt_coroutine_stackless);
    ffrt_task_handle_t maintask=ffrt_submit_h_coroutine((void *)&co1, maintask_exec_stackless_coroutine_fail, \
        maintask_destroy_stackless_coroutine_fail, NULL, NULL, &maintask_attr);
    ffrt_wait();
    ffrt_task_handle_destroy(maintask);
}

TEST_F(CoroutineTest, set_get_coroutine_type_fail)
{
    ffrt_task_attr_t attr;
    ffrt_task_attr_init(&attr);
    ffrt_task_attr_set_name(&attr, "stackless_coroutine");
    ffrt_task_attr_set_coroutine_type(nullptr, ffrt_coroutine_stackless);
    ffrt_task_attr_get_coroutine_type(nullptr);
}