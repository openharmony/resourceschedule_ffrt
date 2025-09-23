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

#ifndef _CAPABILITY_H
#define _CAPABILITY_H

namespace ffrt {
// Check whether the current process enables the capability of 'CAP_SYS_NICE'.
// 'CAP_SYS_NICE' allows processes to set higher priority as follows:
// 1. nice less than RLIMIT_NICE(default 0);
// 2. real-time scheduling mode: (1) SCHED_FIFO, (2) SCHED_RR.
bool CheckProcCapSysNice();
} // namespace ffrt

#endif /* _CAPABILITY_H */