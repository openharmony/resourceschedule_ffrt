/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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
#include "util/white_list.h"
#include "internal_inc/osal.h"
#include "util/ffrt_facade.h"

namespace {
constexpr char CONF_FILEPATH[] = "/etc/ffrt/ffrt_whitelist.conf";
constexpr int INDENT_SPACE_NUM = 4;
constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
}

namespace ffrt {
WhiteList::WhiteList()
{
    LoadFromFile();
}

WhiteList& WhiteList::GetInstance()
{
    static WhiteList instance;
    return instance;
}

bool WhiteList::IsEnabled(const std::string& functionName, bool defaultWhenAbnormal)
{
    if (TryRefreshWhiteListOnInit()) {
        auto it = whiteList_.find(functionName);
        if (it != whiteList_.end()) {
            return it->second;
        }
    } else {
        std::unique_lock lock(whitelistMutex_);
        auto it = whiteList_.find(functionName);
        if (it != whiteList_.end()) {
            return it->second;
        }
    }

    // 若白名单加载失败或不在白名单中的默认返回值
    return defaultWhenAbnormal;
}

void WhiteList::LoadFromFile()
{
    char processNameChar[PROCESS_NAME_BUFFER_LENGTH] {};
    GetProcessName(processNameChar, PROCESS_NAME_BUFFER_LENGTH);
    if (strlen(processNameChar) == 0) {
        FFRT_LOGW("Get process name failed.");
        return;
    }
    std::string processName = std::string(processNameChar);
#ifdef OHOS_STANDARD_SYSTEM
    std::string whiteProcess;
    std::ifstream file(CONF_FILEPATH);
    std::string functionName;
    if (file.is_open()) {
        while (std::getline(file, whiteProcess)) {
            size_t pos = whiteProcess.find("{");
            if (pos != std::string::npos) {
                functionName = whiteProcess.substr(0, pos - 1);
                whiteList_[functionName] = false;
            } else if ((whiteProcess != "}" && whiteProcess != "") &&
                processName.find(whiteProcess.substr(INDENT_SPACE_NUM)) != std::string::npos) {
                whiteList_[functionName] = true;
            }
        }
    } else {
        // 当文件不存在或者无权限时默认都关
        FFRT_LOGW("white_list.conf does not exist or file permission denied");
    }
#else
    whiteList_["IsInSFFRTList"] = false;
    if (processName.find("zygote") != std::string::npos) {
        whiteList_["IsInSFFRTList"] = true;
    }
    if (processName.find("CameraDaemon") != std::string::npos) {
        whiteList_["SetThreadAttr"] = true;
        whiteList_["CreateCPUWorker"] = true;
        whiteList_["HandleTaskNotifyConservative"] = true;
    }
#endif // OHOS_STANDARD_SYSTEM
}

bool WhiteList::TryRefreshWhiteListOnInit()
{
    if (FFRT_LIKELY(whitelistInited_)) {
        return true;
    }

    // 确保在 init 与 facade 初始化之间和之后各构造一次白名单
    if (!ffrt::GetInitFlag()) {
        return false;
    }

    std::lock_guard lock(whitelistMutex_);
    if (!whitelistInited_) {
        LoadFromFile();
        whitelistInited_ = true;
    }
    return true;
}
} // namespace ffrt