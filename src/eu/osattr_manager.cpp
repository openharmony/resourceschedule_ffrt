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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "eu/qos_interface.h"
#include "eu/osattr_manager.h"

namespace ffrt {
bool OSAttrManager::CheckSchedAttrPara(const std::string &name, int min, int max, int paraValue)
{
    if (paraValue < min || paraValue > max) {
        FFRT_LOGE("OSAttrManager::CheckAttrPara para %s is invalid", name.c_str());
        return false;
    }
    return true;
}

int OSAttrManager::UpdateSchedAttr(const QoS& qos, ffrt_os_sched_attr *attr)
{
    return -1;
}

void OSAttrManager::SetCGroupCtlPara(const std::string &name, int32_t value)
{
    const std::string filename = cpuctlGroupIvePath + name;
    SetCGroupPara(filename, value);
}

void OSAttrManager::SetCGroupSetPara(const std::string &name, const std::string &value)
{
    const std::string filename = cpusetGroupIvePath + name;
    SetCGroupPara(filename, value);
}

void OSAttrManager::SetTidToCGroup(int32_t pid)
{
    SetTidToCGroupPrivate(cpuctlGroupIvePath + cpuThreadNode, pid);
    SetTidToCGroupPrivate(cpusetGroupIvePath + cpuThreadNode, pid);
}

void OSAttrManager::SetTidToCGroupPrivate(const std::string &filename, int32_t pid)
{
    constexpr int32_t maxThreadId = 0xffff;
    if (pid <= 0 || pid >= maxThreadId) {
        FFRT_LOGE("[cgroup_ctrl] invalid pid[%d]\n", pid);
        return;
    }
    SetCGroupPara(filename, pid);
}
} // namespace ffrt