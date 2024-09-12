/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#define private public
#define protected public
#include <gtest/gtest.h>
#include <thread>
#include "eu/cpu_monitor.h"
#include "eu/cpu_worker.h"
#include "eu/scpuworker_manager.h"
#include "eu/cpu_manager_interface.h"
#include "eu/worker_thread.h"
#include "qos.h"
#include "common.h"

#undef private
#undef protected

namespace OHOS {
namespace FFRT_TEST {
using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif
using namespace OHOS::FFRT_TEST;
using namespace ffrt;
using namespace std;


class CpuWorkerTest : public testing::Test {
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

/**
 * @tc.name: Start
 * @tc.desc: Test whether the Start interface are normal.
 * @tc.type: FUNC
 */

// 死循环 卡在cpp的line33
}
}
