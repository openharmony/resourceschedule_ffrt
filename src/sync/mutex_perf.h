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

#ifndef _MUTEX_PERF_H_
#define _MUTEX_PERF_H_

// open -O0 -DMUTEX_PERF in Makefile

#include <string>
#include <unordered_map>
#include <mutex>

namespace ffrt {
struct MutexStatistic {
    ~MutexStatistic();
    std::mutex mtx_;
    std::unordered_map<std::string, uint32_t> cycles_;
};

void AddMutexCycles(std::string key, uint32_t val);

namespace xx {
class mutex : public std::mutex {
public:
    mutex() : label_("undefined") {};
    explicit mutex(const std::string& label) : label_(label) {};
    ~mutex() = default;

    inline void lock()
    {
#ifdef __x86_64__
        uint32_t t_s = __builtin_ia32_rdtsc();
#endif
        impl_.lock();
#ifdef __x86_64__
        uint32_t t_e = __builtin_ia32_rdtsc();
        AddMutexCycles(label_, t_e - t_s);
#endif
    }

    inline void unlock()
    {
#ifdef __x86_64__
        uint32_t t_s = __builtin_ia32_rdtsc();
#endif
        impl_.unlock();
#ifdef __x86_64__
        uint32_t t_e = __builtin_ia32_rdtsc();
        AddMutexCycles(label_, t_e - t_s);
#endif
    }

private:
    std::string label_;
    std::mutex impl_;
};
} // namespace xx
} // namespace ffrt

#endif