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

#ifndef FFRT_QOS_H
#define FFRT_QOS_H

#include "ffrt.h"

namespace ffrt {
class QoS {
public:
    QoS(enum qos qos = qos_default)
    {
        if (qos < qos_inherit) {
            qos = qos_inherit;
        } else if (qos > qos_defined_ive) {
            qos = qos_defined_ive;
        }

        this->qos = qos;
    }

    QoS(const QoS& qos) : qos(qos.qos)
    {
    }

    QoS(int qos) : QoS(static_cast<enum qos>(qos))
    {
    }

    enum qos operator()() const
    {
        return qos;
    }

    QoS& operator=(enum qos qos)
    {
        this->qos = qos;
        return *this;
    }

    QoS& operator=(const QoS& qos)
    {
        if (this != &qos) {
            this->qos = qos.qos;
        }
        return *this;
    }

    bool operator==(enum qos qos) const
    {
        return this->qos == qos;
    }

    bool operator==(const QoS& qos) const
    {
        return this->qos == qos.qos;
    }

    bool operator!=(enum qos qos) const
    {
        return !(*this == qos);
    }

    bool operator!=(const QoS& qos) const
    {
        return !(*this == qos);
    }

    operator int() const
    {
        return static_cast<int>(qos);
    }

    static constexpr int Min()
    {
        return qos_unspecified;
    }

    static constexpr int Max()
    {
        return qos_defined_ive + 1;
    }

private:
    enum qos qos;
};

}; // namespace ffrt

#endif