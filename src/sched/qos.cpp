/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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
#include "qos.h"

namespace ffrt {
int QoSMap(int qos)
{
    if (qos <= static_cast<int>(qos_inherit)) {
        return qos_inherit;
    } else if (qos >= static_cast<int>(qos_background) &&
        qos <= static_cast<int>(qos_max)) {
        return qos;
    } else {
        return qos_default;
    }
}

static Func_qos_map func_qos_map = nullptr;
void SetFuncQosMap(Func_qos_map func)
{
    func_qos_map = func;
}

Func_qos_map GetFuncQosMap(void)
{
    return func_qos_map;
}

int QoSMax(void)
{
    return qos_max + 1;
}

static Func_qos_max func_qos_max = nullptr;
void SetFuncQosMax(Func_qos_max func)
{
    func_qos_max = func;
}

Func_qos_max GetFuncQosMax(void)
{
    return func_qos_max;
}
}