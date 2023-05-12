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
#ifndef FFRT_API_CPP_SLEEP_H
#define FFRT_API_CPP_SLEEP_H
#include <ctime>
#include <chrono>
#include "c/sleep.h"

namespace ffrt {
namespace this_task {
static inline void yield()
{
    ffrt_yield();
}

template <class _Rep, class _Period>
inline void sleep_for(const std::chrono::duration<_Rep, _Period>& d)
{
    auto secs = std::chrono::duration_cast<std::chrono::seconds>(d);
    auto nanosecs = std::chrono::duration_cast<std::chrono::nanoseconds>(d - secs);
    timespec ts{secs.count(), static_cast<long>(nanosecs.count())};
    ffrt_sleep(&ts);
}
} // namespace this_task
} // namespace ffrt
#endif
