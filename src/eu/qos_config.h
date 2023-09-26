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

#ifndef QOS_CONFIG_H
#define QOS_CONFIG_H
#include "qos_interface.h"

namespace ffrt {
constexpr int PRIO_AFFI_SET_DISABLE = 0;
constexpr int PRIO_AFFI_SET_ENABLE = 3;
class QosConfig {
public:
    QosConfig();

    ~QosConfig() {}

    static QosConfig& Instance()
    {
        static QosConfig instance;
        return instance;
    }

    void setPolicySystem();

    struct QosPolicyDatas& getPolicySystem()
    {
        return g_systemServerQosPolicy;
    }

    void setThreadCtrls();

    struct ThreadAttrCtrlDatas& getThreadCtrls()
    {
        return threadCtrls;
    }

private:
    struct QosPolicyDatas g_systemServerQosPolicy;
    struct ThreadAttrCtrlDatas threadCtrls;
};
}
#endif /* QOS_CONFIG_H */