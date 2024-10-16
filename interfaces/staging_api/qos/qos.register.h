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
#ifndef STAGING_QOS_REGISTER_H
#define STAGING_QOS_REGISTER_H
#include <cstdint>
namespace ffrt {

constexpr int BIND_SMALL_CROES = 0b1;
constexpr int BIND_MIDDLE_CROES = 0b10;
constexpr int BIND_BIG_CROES = 0b100;
constexpr int NO_BIND_CROES = 0b0;

// expect_qos must in ffrt_inner_qos_default_t or ffrt_qos_default_t
int qos_register(int expect_qos, int bind_cores);
}
#endif