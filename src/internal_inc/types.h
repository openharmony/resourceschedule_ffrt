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

#ifndef FFRT_TYPES_HPP
#define FFRT_TYPES_HPP

namespace ffrt {
#ifdef ASAN_MODE
constexpr bool USE_COROUTINE = false;
#else
constexpr bool USE_COROUTINE = true;
#endif
enum DT {
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    FP16,
    FP32,
    FP64,
};

enum class DevType {
    CPU,
    DEVMAX,
};

enum class TaskType {
    ROOT,
    DEFAULT,
};

enum class DataStatus {
    IDLE, // 默认状态
    READY, // 当前版本被生产出来，标志着这个版本的所有消费者可以执行
    CONSUMED, // 同时也是RELEASE，当前版本的所有消费者已经执行完成，标志着下一个版本的生产者可以执行
    MERGED, // 嵌套场景下，标志一个子任务的version已经被父任务的version合并
};

enum class NestType {
    DEFAULT, // 不存在嵌套关系
    PARENTOUT, // 同parent的输出嵌套
    PARENTIN, // 同parent的输入嵌套
};

enum class TaskStatus {
    CONSTRUCTING, // 预留，暂不使用
    SUBMITTING, // 预留，暂不使用
    PENDING, // 默认状态
    READY, // 可以被执行，可能由于上一个任务执行完而被依赖管理模块触发进入该状态（PEND->READY），也可能submit时被依赖管理模块触发进入该状态（PEND->READY）
    EXECUTING, // 被调度器调度，正在执行，可能由调度模块触发进入该状态（READY->EXE），还可能由wait成功时被依赖管理模块触发进入该状态（BLOCK->EXE）
    EXECUTING_NO_DEPENDENCE, // 执行过程中已经解依赖
    BLOCKED, // 进入wait状态，wait时被依赖管理模块触发进入该状态（EXE->BLOCK）
    FINISH, // 干完了（EXEC->FINISH）
    RELEASED, // 预留，暂不使用
};

#ifdef FFRT_IO_TASK_SCHEDULER
typedef enum {
    ET_PENDING, // executor_task 非入队状态
    ET_EXECUTING, // executor_task 执行状态
    ET_TOREADY, // executor_task 等待wake通知
    ET_READY, // executor_task 入队状态
    ET_FINISH, // executor_task 执行完成，准备执行回调+销毁
} ExecTaskStatus;
#endif

enum class Denpence {
    DEPENCE_INIT,
    DATA_DEPENCE,
    CALL_DEPENCE,
    CONDITION_DEPENCE,
};

enum class SpecTaskType {
    EXIT_TASK,
    SLEEP_TASK,
    SPEC_TASK_MAX,
};

enum SkipStatus {
    SUBMITTED,
    EXECUTED,
    SKIPPED,
};
#ifndef _MSC_VER
#define FFRT_LIKELY(x) (__builtin_expect(!!(x), 1))
#define FFRT_UNLIKELY(x) (__builtin_expect(!!(x), 0))
#else
#define FFRT_LIKELY(x) (x)
#define FFRT_UNLIKELY(x) (x)
#endif

#define FORCE_INLINE
} // namespace ffrt
#endif
