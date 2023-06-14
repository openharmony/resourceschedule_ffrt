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
class QosConfig {
public:
    QosConfig();

    ~QosConfig() {}

    static QosConfig& Instance()
    {
        static QosConfig instance;
        return instance;
    }

    void setPolicyDefault();

    struct QosPolicyDatas& getPolicyDefault()
    {
        return g_defaultQosPolicy;
    }

    void setPolicyForeground();

    struct QosPolicyDatas& getPolicyForeground()
    {
        return g_foregroundQosPolicy;
    }

    void setPolicyBackground();

    struct QosPolicyDatas& getPolicyBackground()
    {
        return g_backgroundQosPolicy;
    }

    void setPolicySystem();

    struct QosPolicyDatas& getPolicySystem()
    {
        return g_systemServerQosPolicy;
    }
private:
    struct QosPolicyDatas g_defaultQosPolicy;
    struct QosPolicyDatas g_foregroundQosPolicy;
    struct QosPolicyDatas g_backgroundQosPolicy;
    struct QosPolicyDatas g_systemServerQosPolicy;
};
}
#endif /* QOS_CONFIG_H */