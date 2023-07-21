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
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <array>
#include "eu/qos_interface.h"
#include "dfx/log/ffrt_log_api.h"
#include "eu/osattr_manager.h"

namespace ffrt {
const int fd_buffer_len = 20;

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
    FFRT_LOGI("OSAttrManager::UpdateSchedAttr start qos[%d] attr.lat_nice[%d] attr.cpumap[0x%s] attr.u_min[%d]\
        attr.shares[%d]", qos(), attr->latency_nice, attr->cpumap, attr->uclamp_min, attr->shares);
    if (qos() <= qos_max) {
        FFRT_LOGE("qos[%d] attr update is not supported.\n", qos());
        return -1;
    }

    struct SchedParaCheckInfo {
        std::string paraName;
        int min;
        int max;
        int paraValue;
    };

    std::vector<SchedParaCheckInfo> checkInfo {
        { "share",        CGROUP_SHARES_MIN,    CGROUP_SHARES_MAX,     attr->shares},
        { "latencynice",  CGROUP_LAT_NICE_MIN,  CGROUP_LAT_NICE_MAX,   attr->latency_nice},
        { "uclampmin",    CGROUP_UCLAMP_MIN,    CGROUP_UCLAMP_MAX,     attr->uclamp_min},
        { "uclampmax",    CGROUP_UCLAMP_MIN,    CGROUP_UCLAMP_MAX,     attr->uclamp_max},
        { "vipprio",      CGROUP_VIPPRIO_MIN,   CGROUP_VIPPRIO_MAX,    attr->vip_prio},
    };

    for (const auto &tmpPara : checkInfo) {
        if (!CheckSchedAttrPara(tmpPara.paraName, tmpPara.min, tmpPara.max, tmpPara.paraValue)) {
            return -1;
        }
    }

    SetCGroupCtlPara(cpuSharesNode, attr->shares);
    SetCGroupCtlPara(cpuLatencyniceNode, attr->latency_nice);
    SetCGroupCtlPara(cpuUclampminNode, attr->uclamp_min);
    SetCGroupCtlPara(cpuUclampmaxNode, attr->uclamp_max);
    SetCGroupCtlPara(cpuvipprioNode, attr->vip_prio);
    SetCGroupSetPara(cpuMapNode, static_cast<std::string>(attr->cpumap));
    return 0;
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

template <typename T>
void OSAttrManager::SetCGroupPara(const std::string &filename, T& value)
{
    char filePath[PATH_MAX_LENS] = {0};
    if (filename.empty()) {
        FFRT_LOGE("[cgroup_ctrl] invalid para, filename is empty");
        return;
    }

    if ((strlen(filename.c_str()) > PATH_MAX_LENS) || (realpath(filename.c_str(), filePath) == nullptr)) {
        FFRT_LOGE("[cgroup_ctrl] invalid file path:%s, error:%s\n", filename.c_str(), strerror(errno));
        return;
    }

    int32_t fd = open(filePath, O_RDWR);
    if (fd < 0) {
        FFRT_LOGE("[cgroup_ctrl] fail to open filePath:%s", filePath);
        return;
    }

    std::string valueStr;
    if constexpr (std::is_same<T, int32_t>::value) {
        valueStr = std::to_string(value);
    } else if constexpr (std::is_same<T, const std::string>::value) {
        valueStr = value;
    } else {
        FFRT_LOGE("[cgroup_ctrl] invalid value type\n");
        close(fd);
        return;
    }

    int32_t ret = write(fd, valueStr.c_str(), valueStr.size());
    if (ret < 0) {
        FFRT_LOGE("[cgroup_ctrl] fail to write path:%s valueStr:%s to fd:%d, errno:%d",
            filePath, valueStr.c_str(), fd, errno);
        close(fd);
        return;
    }

    const uint32_t bufferLen = fd_buffer_len;
    std::array<char, bufferLen> buffer {};
    int32_t count = read(fd, buffer.data(), bufferLen);
    if (count <= 0) {
        FFRT_LOGE("[cgroup_ctrl] fail to read value:%s to fd:%d, errno:%d", buffer.data(), fd, errno);
    } else {
        FFRT_LOGI("[cgroup_ctrl] success to read %s buffer:%s", filePath, buffer.data());
    }
    close(fd);
}
} // namespace ffrt