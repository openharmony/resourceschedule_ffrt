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

#if defined(__aarch64__)
constexpr size_t STACK_MAGIC = 0x7BCDABCDABCDABCD;
#elif defined(__arm__)
constexpr size_t STACK_MAGIC = 0x7BCDABCD;
#elif defined(__x86_64__)
constexpr size_t STACK_MAGIC = 0x7BCDABCDABCDABCD;
#elif defined(__riscv) && __riscv_xlen == 64
constexpr size_t STACK_MAGIC = 0x7BCDABCDABCDABCD;
#endif

namespace ffrt {
class CPUEUTask;
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

enum class BlockType {
    BLOCK_COROUTINE,
    BLOCK_THREAD
};

#if defined(__aarch64__)
    constexpr uint64_t STACK_SIZE = 1 << 20; // 至少3*PAGE_SIZE
#elif defined(__arm__)
    constexpr uint64_t STACK_SIZE = 1 << 20;
#else
    constexpr uint64_t STACK_SIZE = 1 << 20;
#endif

using CoCtx = struct co2_context;

struct CoRoutineEnv {
    CoRoutine* runningCo;
    CoCtx schCtx;
    const std::function<bool(ffrt::CPUEUTask*)>* pending;
};

struct StackMem {
    uint64_t size;
    size_t magic;
    uint8_t stk[8];
};

struct CoRoutine {
    std::atomic_int status;
    CoRoutineEnv* thEnv;
    ffrt::CPUEUTask* task;
    CoCtx ctx;
    bool legacyMode = false;
    BlockType blockType = BlockType::BLOCK_COROUTINE;
    bool isTaskDone = false;
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

class CoRoutineFactory {
public:
    using CowakeCB = std::function<void (ffrt::CPUEUTask*, bool)>;

    static CoRoutineFactory &Instance();

    static void CoWakeFunc(ffrt::CPUEUTask* task, bool timeOut)
    {
        return Instance().cowake_(task, timeOut);
    }

    static void RegistCb(const CowakeCB &cowake)
    {
        Instance().cowake_ = cowake;
    }
private:
    CowakeCB cowake_;
};

void CoStackFree(void);
void CoWorkerExit(void);

void CoStart(ffrt::CPUEUTask* task);
void CoYield(void);

void CoWait(const std::function<bool(ffrt::CPUEUTask*)>& pred);
void CoWake(ffrt::CPUEUTask* task, bool timeOut);

#ifdef FFRT_TASK_LOCAL_ENABLE
void TaskTsdDeconstruct(ffrt::CPUEUTask* task);
#endif

#endif