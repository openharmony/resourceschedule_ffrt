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

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <gtest/gtest.h>
#include <securec.h>
#include "dfx/log/ffrt_log_api.h"
#include "faultloggerd_client.h"
#include "dfx/bbox/fault_logger_fd_manager.h"
#include "../common.h"

static const char* FILE_NAME = "testLogToFaultlogger.txt";

using namespace testing;
#ifdef HWTEST_TESTING_EXT_ENABLE
using namespace testing::ext;
#endif

class FfrtLogTest : public testing::Test {
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

// FaultLogger获取句柄/写入日志/读取日志/关闭句柄
HWTEST_F(FfrtLogTest, faultLoggerFdManager, TestSize.Level1)
{
    // 获取句柄
    FaultLoggerFdManager manager = FaultLoggerFdManager::Instance();
    EXPECT_EQ(manager.GetFaultLoggerFd(), -1);
    EXPECT_TRUE(manager.InitFaultLoggerFd() > 0);

    // 写入日志
    manager.WriteFaultLogger("test logToFaultlogger arg1[%s],arg2[%d]", "ARG1", 1);
    manager.WriteFaultLogger("test logToFaultlogger arg1[%s],arg2[%d]", "ARG1", 1);

    // 关闭句柄
    manager.CloseFd();
    EXPECT_EQ(manager.GetFaultLoggerFd(), -1);

    // 读取日志
    EXPECT_TRUE(manager.InitFaultLoggerFd() > 0);
    int fd = manager.GetFaultLoggerFd();
    const int bufferLen = 2048;
    std::array<char, bufferLen> readBuf {};
    int readSize = read(fd, readBuf.data(), bufferLen);
    std::string msg = "test logToFaultlogger arg1[ARG1],arg2[1]\ntest logToFaultlogger arg1[ARG1],arg2[1]\n";
    EXPECT_EQ(readSize, msg.size());
    EXPECT_EQ(readBuf.data(), msg);
    close(fd);

    // 删除文件
    remove(FILE_NAME);
}