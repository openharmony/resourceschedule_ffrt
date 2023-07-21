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

#ifndef OS_ATTR_MANAGER_H
#define OS_ATTR_MANAGER_H
#include "ffrt.h"
#include "sched/qos.h"

namespace ffrt {
const std::string cpuctlGroupIvePath = "/dev/cpuctl/cam2stage";
const std::string cpusetGroupIvePath = "/dev/cpuset/cam2stage";
const std::string cpuThreadNode = "/tasks";
const std::string cpuSharesNode = "/cpu.shares";
const std::string cpuLatencyniceNode = "/cpu.latency.nice";
const std::string cpuUclampminNode = "/cpu.uclamp.min";
const std::string cpuUclampmaxNode = "/cpu.uclamp.max";
const std::string cpuvipprioNode = "/cpu.vip_prio";
const std::string cpuMapNode = "/cpus";
const int PATH_MAX_LENS = 4096;
const int CGROUP_SHARES_MIN = 2;
const int CGROUP_SHARES_MAX = 262144;
const int CGROUP_LAT_NICE_MIN = -20;
const int CGROUP_LAT_NICE_MAX = 19;
const int CGROUP_UCLAMP_MIN = 0;
const int CGROUP_UCLAMP_MAX = 100;
const int CGROUP_VIPPRIO_MIN = 0;
const int CGROUP_VIPPRIO_MAX = 10;
class OSAttrManager {
public:
    OSAttrManager() {}
    ~OSAttrManager() {}

    static inline OSAttrManager* Instance()
    {
        static OSAttrManager instance;
        return &instance;
    }

    bool CheckSchedAttrPara(const std::string &name, int min, int max, int paraValue);
    int UpdateSchedAttr(const QoS& qos, ffrt_os_sched_attr *attr);
    void SetCGroupCtlPara(const std::string &name, int32_t value);
    void SetCGroupSetPara(const std::string &name, const std::string &value);
    void SetTidToCGroup(int32_t pid);
    void SetTidToCGroupPrivate(const std::string &filename, int32_t pid);
    template <typename T>
    void SetCGroupPara(const std::string &filename, T& value);
};
} // namespace ffrt

#endif /* OS_ATTR_MANAGER_H */