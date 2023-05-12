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
#include "eu/qos_policy.h"
#include "dfx/log/ffrt_log_api.h"
#include "eu/osattr_manager.h"

namespace ffrt {
const int fd_buffer_len = 20;

int OSAttrManager::UpdateSchedAttr(enum qos qos, ffrt_os_sched_attr *attr)
{
    FFRT_LOGI("OSAttrManager::UpdateSchedAttr start qos[%d] attr.lat_nice[%d] attr.cpumap[0x%s] attr.u_min[%d]\
        attr.shares[%d]", qos, attr->latency_nice, attr->cpumap, attr->uclamp_min, attr->shares);
    if (qos != qos_defined_ive) {
        FFRT_LOGE("qos[%d] attr update is not supported.\n", static_cast<int>(qos));
        return -1;
    }

    SetCGroupCtlPara(cpuctlGroupIvePath, cpuSharesNode, attr->shares);
    SetCGroupCtlPara(cpuctlGroupIvePath, cpuLatencyniceNode, attr->latency_nice);
    SetCGroupCtlPara(cpuctlGroupIvePath, cpuUclampminNode, attr->uclamp_min);
    SetCGroupSetPara(cpusetGroupIvePath, cpuMapNode, static_cast<std::string>(attr->cpumap));
    return 0;
}

void OSAttrManager::SetCGroupCtlPara(const std::string &path, const std::string &name, int32_t value)
{
    const std::string filename = path + name;
    char filePath[PATH_MAX_LENS] = {0};
    if (filename.empty()) {
        FFRT_LOGE("invalid parai,filename is empty");
        return;
    }

    if ((strlen(filename.c_str()) > PATH_MAX_LENS) || (realpath(filename.c_str(), filePath) == nullptr)) {
        FFRT_LOGE("invalid file path:%s, error:%s\n", filename.c_str(), strerror(errno));
        return;
    }

    int32_t fd = open(filePath, O_RDWR);
    if (fd < 0) {
        FFRT_LOGE("fail to open cgroup Path:%s, fd=%d, errno=%d", filePath, fd, errno);
        return;
    }

    const std::string valueStr = std::to_string(value);
    int32_t ret = write(fd, valueStr.c_str(), valueStr.size());
    if (ret < 0) {
        FFRT_LOGE("fail to write valueStr:%s to fd:%d, errno:%d", valueStr.c_str(), fd, errno);
    }
    close(fd);

    fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        FFRT_LOGE("fail to open cgroup Path:%s, errno=%d", filePath, errno);
        return;
    }
    const uint32_t bufferLen = fd_buffer_len;
    std::array<char, bufferLen> buffer {};
    int32_t count = read(fd, buffer.data(), bufferLen);
    if (count <= 0) {
        FFRT_LOGE("fail to read valueStr:%s to fd:%d, errno:%d", buffer.data(), fd, errno);
    } else {
        FFRT_LOGE("[%d]success to read %s buffer:%s", __LINE__, name.c_str(), buffer.data());
    }
    close(fd);
}

void OSAttrManager::SetCGroupSetPara(const std::string &path, const std::string &name, const std::string &value)
{
    const std::string filename = path + name;
    char filePath[PATH_MAX_LENS] = {0};
    if (filename.empty()) {
        FFRT_LOGE("invalid para,filename is empty");
        return;
    }

    if ((strlen(filename.c_str()) > PATH_MAX_LENS) || (realpath(filename.c_str(), filePath) == nullptr)) {
        FFRT_LOGE("invalid file path:%s, error:%s\n", filename.c_str(), strerror(errno));
        return;
    }

    int32_t fd = open(filePath, O_RDWR);
    if (fd < 0) {
        FFRT_LOGE("fail to open cgroup cpus Path:%s, fd=%d, errno=%d", filePath, fd, errno);
        return;
    }

    int32_t ret = write(fd, value.c_str(), value.size());
    if (ret < 0) {
        FFRT_LOGE("fail to write value:%s to fd:%d, errno:%d", value.c_str(), fd, errno);
    }
    close(fd);

    fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        FFRT_LOGE("fail to open cgroup cpus Path:%s, errno=%d", filePath, errno);
        return;
    }
    const uint32_t bufferLen = fd_buffer_len;
    std::array<char, bufferLen> buffer {};
    int32_t count = read(fd, buffer.data(), bufferLen);
    if (count <= 0) {
        FFRT_LOGE("fail to read value:%s to fd:%d, errno:%d", buffer.data(), fd, errno);
    } else {
        FFRT_LOGE("[%d]success to read %s buffer:%s", __LINE__, name.c_str(), buffer.data());
    }
    close(fd);
}
} // namespace ffrt