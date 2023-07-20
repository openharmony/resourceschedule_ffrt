/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "qos_config.h"

namespace ffrt {

constexpr uint8_t THREAD_AFFINITY_LIT     = 0x0F;
constexpr uint8_t THREAD_AFFINITY_MID     = 0x70;
constexpr uint8_t THREAD_AFFINITY_BIG     = 0x80;
constexpr uint8_t THREAD_AFFINITY_LIT_MID = 0x7F;
constexpr uint8_t THREAD_AFFINITY_LIT_BIG = 0x8F;
constexpr uint8_t THREAD_AFFINITY_MID_BIG = 0xF0;
constexpr uint8_t THREAD_AFFINITY_ALL     = 0xFF;

constexpr uint8_t THREAD_PRIO_NORMAL_MIN         = 139;
constexpr uint8_t THREAD_PRIO_NORMAL_BACKGROUND  = 130;
constexpr uint8_t THREAD_PRIO_NORMAL_UNFAVORABLE = 125;
constexpr uint8_t THREAD_PRIO_NORMAL_DEFAULT     = 120;
constexpr uint8_t THREAD_PRIO_NORMAL_FAVORABLE   = 119;
constexpr uint8_t THREAD_PRIO_NORMAL_FOREGROUND  = 110;
constexpr uint8_t THREAD_PRIO_NORMAL_URGENT      = 105;
constexpr uint8_t THREAD_PRIO_NORMAL_MAX         = 100;
constexpr uint8_t THREAD_PRIO_VIP_MAX            = 89;
constexpr uint8_t THREAD_PRIO_RT_HIGHEST         = 88;

QosConfig::QosConfig()
{
    setPolicySystem();
}

void QosConfig::setPolicySystem()
{
    g_systemServerQosPolicy = {
        .policyType = QOS_POLICY_SYSTEM_SERVER,
        .policyFlag = QOS_FLAG_ALL,
        .policys = {
            {10, 0, 250, THREAD_AFFINITY_LIT, THREAD_PRIO_NORMAL_BACKGROUND, 0, 0},
            {10, 0, 250, THREAD_AFFINITY_ALL, THREAD_PRIO_NORMAL_UNFAVORABLE, 0, 0},
            {0, 0, 1024, THREAD_AFFINITY_ALL, THREAD_PRIO_NORMAL_DEFAULT, 0, 0},
            {0, 0, 1024, THREAD_AFFINITY_ALL, THREAD_PRIO_NORMAL_FOREGROUND, 0, 0},
            {-10, 0, 1024, THREAD_AFFINITY_ALL, THREAD_PRIO_RT_HIGHEST, 1, 1},
            {-20, 0, 1024, THREAD_AFFINITY_MID_BIG, THREAD_PRIO_RT_HIGHEST, 1, 1},
        }
    };
}
}