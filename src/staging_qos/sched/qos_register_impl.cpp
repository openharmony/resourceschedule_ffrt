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
 #include "qos_register_impl.h"
 #include "c/type_def_ext.h"
 #include "dfx/log/ffrt_log_api.h"
 #include "internal_inc/osal.h"

namespace ffrt {
API_ATTRIBUTE((visibility("default")))
int qos_register(int expect_qos, int bind_cores)
{
    return QosRegister::Instance()->Register(expect_qos, bind_cores);
}

QosRegister::QosRegister()
{
    for (int i = 0; i < MAX_REGISTER_QOS_NUM; ++i) {
        qosTable_[i].expectQos = ffrt_qos_inherit;
        qosTable_[i].bindCores = false;
    }
}

int QosRegister::Register(int expectQos, int bindCores)
{
    if (expectQos > qos_max || expectQos <= ffrt_qos_inherit) {
        FFRT_LOGE("expectQos is incalid, expectQos %d", expectQos);
        return -1;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    if (count_ >= MAX_REGISTER_QOS_NUM) {
        FFRT_LOGE("register count exceed 64, count_ %d", count_);
        return -1;
    }
    int customQos = count_ + qos_max + 1;
    qosTable_[count_].expectQos = expectQos;
    qosTable_[count_].bindCores = bindCores;
    FFRT_LOGD("register, expectQos: %d, bindCores: %d, count_: %d", expectQos, bindCores, count_);
    count++;
    return customQos;
}

unsigned long QosRegister::GetAffinityByBindCores(int bindCores)
{
    unsigned long affinity = 0x0;
    switch (bindCores) {
        case 0b1:
            affinity = 0xf;
            break;
        case 0b10:
            affinity = 0x70;
            break;
        case 0b100:
            affinity = 0x80;
            break;
        case 0b110:
            affinity = 0xf0;
            break;
        default:
            break;
    }
    return affinity;
}

unsigned long QosRegister::GetAffinity(int customQos)
{
    int index = customQos - qos_max - 1;
    if (index >= MAX_REGISTER_QOS_NUM) {
        FFRT_LOGE("custom qos exceed max qos, customQos %d", customQos);
        return false;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    return GetAffinityByBindCores(qoeTable_[index].bindCores);
}

int QosRegister::GetExpectQos(int customQos)
{
    int index = customQos - qos_max - 1;
    if (index >= MAX_REGISTER_QOS_NUM) {
        FFRT_LOGE("custom qos exceed max qos, customQos %d", customQos);
        return -1;
    }

    std::unique_lock<std::mutex> lck(mutex_);
    return qoeTable_[index].expectQos;
}
}