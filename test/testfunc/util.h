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

#ifndef FFRT_TESTCASE_UTIL_HPP
#define FFRT_TESTCASE_UTIL_HPP

#include <chrono>
#include <functional>

static inline void stall_us(size_t us)
{
    auto start = std::chrono::steady_clock::now();
    size_t passed = 0;
    while (passed < us) {
        passed =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
    }
}
/**
 * @brief Get the proc memory
 *
 * @param pid
 * @return uint32_t KB
 */
uint32_t get_proc_memory(pid_t pid); // KB

void set_cur_process_cpu_affinity(int cpuid);

void set_cur_thread_cpu_affinity(int cpuid);

/*
 * perf event counter
 */
struct perf_event2_read_format {
    uint64_t nr; // counter num=2
    uint64_t values[2]; // counter value
};
int perf_event2(std::function<void()> func, struct perf_event2_read_format& data, uint32_t event1, uint32_t event2);

int perf_single_event(std::function<void()> func, size_t& count, uint32_t event);

int perf_event_instructions(std::function<void()> func, size_t& count);

int perf_event_cycles(std::function<void()> func, size_t& count);

int perf_event_branch_instructions(std::function<void()> func, size_t& count);

int perf_event_branch_misses(std::function<void()> func, size_t& count);

#endif