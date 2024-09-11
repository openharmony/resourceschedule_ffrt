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
#include "dfx/log/ffrt_log_api.h"
#include "func_pool.h"
#include "internal_inc/osal.h"
#include "../common.h"

using namespace std;
using namespace ffrt;
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class MemTest : public testing::Test {
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

HWTEST_F(MemTest, mem_leakage_test_try_times_min, TestSize.Level1)
{
    uint32_t count = 10;
    uint32_t try_times_max = 5;
    uint32_t try_times = 0;
    uint32_t pid = GetPid();
    uint32_t start_mem;
    uint32_t end_mem;
    uint32_t mem_inc = 512; // KB

    while (try_times < try_times_max) {
        try_times++;
        start_mem = get_proc_memory(pid);
        NestedWhile(count);
        end_mem = get_proc_memory(pid);
        if (end_mem < start_mem + mem_inc) {
            break;
        }
    }

    printf("mem_leakage_test_try_times_min try_times:%u start_mem:%uKB end_mem:%uKB\n", try_times, start_mem, end_mem);
    EXPECT_LT(try_times, try_times_max);
}