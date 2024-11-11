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
#include <cstdlib>
#include "../common.h"

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class InitTest : public testing::Test {
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

class Env {
public:
    Env()
    {
        putenv("FFRT_PATH_HARDWARE=1");
    }
    ~Env()
    {
        putenv("FFRT_PATH_HARDWARE=0");
    }
};

Env g_env __attribute__ ((init_priority(102)));
HWTEST_F(InitTest, hardware_test, TestSize.Level1)
{
    int x = 0;
    auto h = ffrt::submit_h(
        [&]() {
            x++;
        }, {}, {});
    ffrt::wait({h});
    EXPECT_EQ(x, 1);
}