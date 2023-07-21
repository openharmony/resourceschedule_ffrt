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

#ifndef QOS_POLICY_H
#define QOS_POLICY_H
#include "qos_interface.h"

namespace ffrt {
constexpr int MAX_RT_PRIO = 89;
constexpr int MAX_VIP_PRIO = 99;
constexpr int DEFAULT_PRIO = 120;
int SetAffinity(unsigned long affinity, int tid);
void SetPriority(unsigned char priority, WorkerThread* thread);
}
#endif /* QOS_POLICY_H */