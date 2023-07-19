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
    QoS(int qos = qos_default)
    {
        if (qos < qos_inherit) {
            qos = qos_inherit;
        } else if (qos > qos_max) {
            qos = qos_max;
        }

        qos_ = qos;
    }

    QoS(const QoS& qos) : qos_(qos())
    {
    }

    int operator()() const
    {
        return qos_;
    }

    QoS& operator=(int qos)
    {
        qos_ = qos;
        return *this;
    }

    QoS& operator=(const QoS& qos)
    {
        if (this != &qos) {
            qos_ = qos();
        }
        return *this;
    }

    bool operator==(int qos) const
    {
        return qos_ == qos;
    }

    bool operator==(const QoS& qos) const
    {
        return qos_ == qos();
    }

    bool operator!=(int qos) const
    {
        return !(*this == qos);
    }

    bool operator!=(const QoS& qos) const
    {
        return !(*this == qos);
    }

    operator int() const
    {
        return qos_;
    }

    static constexpr int Min()
    {
        return qos_background;
    }

    static constexpr int Max()
    {
        return qos_max;
    }

private:
    int qos_;
};
}; // namespace ffrt
#endif