/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef FFRT_QOS_REGISTER_IMPL_H
#define FFRT_QOS_REGISTER_IMPL_H
#include <cstdint>
#include <mutex>
namespace ffrt {
constexpr int MAX_REGISTER_QOS_NUM = 8;
constexpr int MAX_BIND_COMBINATION = 6;
class QosRegister {
public:
    QosRegister();
    ~QosRegister() = default;
    static inline QosRegister* Instance()
    {
        static QosRegister ins;
        return &ins;
    }
    int Register(int expectQos, int bindCores, int priority);
    unsigned long GetAffinity(int customQos);
    int GetPriority(int customQos);
    int GetExpectQos(int customQos);

private:
    struct QosAttr {
        int expectQos;
        int bindCores;
        int priority;
    };
    QosAttr qosTable_[MAX_REGISTER_QOS_NUM];
    uint8_t count_ = 0;
    std::mutex mutex_;
    unsigned long GetAffinityByBindCores(int bindCores);
};
}
#endif