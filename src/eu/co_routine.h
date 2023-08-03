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

#ifndef FFRT_CO_ROUTINE_HPP
#define FFRT_CO_ROUTINE_HPP
#include <functional>
#include <atomic>
#include "co2_context.h"
#include "core/task_io.h"
#if defined(__aarch64__)
constexpr size_t STACK_MAGIC = 0x7BCDABCDABCDABCD;
#elif defined(__arm__)
constexpr size_t STACK_MAGIC = 0x7BCDABCD;
#elif defined(__x86_64__)
constexpr size_t STACK_MAGIC = 0x7BCDABCDABCDABCD;
#endif

namespace ffrt {
struct TaskCtx;
struct WaitEntry;
} // namespace ffrt
struct CoRoutine;

enum class CoStatus {
    CO_UNINITIALIZED,
    CO_NOT_FINISH,
    CO_RUNNING,
};

enum class CoStackProtectType {
    CO_STACK_WEAK_PROTECT,
    CO_STACK_STRONG_PROTECT
};

#if defined(__aarch64__)
    constexpr uint64_t STACK_SIZE = 1 << 20; // 至少3*PAGE_SIZE
#elif defined(__arm__)
    constexpr uint64_t STACK_SIZE = 1 << 15;
#else
    constexpr uint64_t STACK_SIZE = 1 << 20;
#endif

using CoCtx = struct co2_context;

struct CoRoutineEnv {
    CoRoutine* runningCo;
    CoCtx schCtx;
    const std::function<bool(ffrt::TaskCtx*)>* pending;
};

struct StackMem {
    uint64_t size;
    size_t magic;
    uint8_t stk[8];
};

struct CoRoutine {
    std::atomic_int status;
    CoRoutineEnv* thEnv;
    ffrt::TaskCtx* task;
    CoCtx ctx;
    StackMem stkMem;
};

struct CoStackAttr {
public:
    explicit CoStackAttr(uint64_t coSize = STACK_SIZE, CoStackProtectType coType =
        CoStackProtectType::CO_STACK_WEAK_PROTECT)
    {
        size = coSize;
        type = coType;
    }
    ~CoStackAttr() {}
    uint64_t size;
    CoStackProtectType type;

    static inline CoStackAttr* Instance(uint64_t coSize = STACK_SIZE,
        CoStackProtectType coType = CoStackProtectType::CO_STACK_WEAK_PROTECT)
    {
        static CoStackAttr inst(coSize, coType);
        return &inst;
    }
};

void CoWorkerExit(void);

void CoStart(ffrt::TaskCtx* task);
void CoYield(void);

void CoWait(const std::function<bool(ffrt::TaskCtx*)>& pred);
void CoWake(ffrt::TaskCtx* task, bool timeOut);
#endif
