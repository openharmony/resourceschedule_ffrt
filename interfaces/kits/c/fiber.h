/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

/**
 * @addtogroup FFRT
 * @{
 *
 * @brief Provides Function Flow Runtime (FFRT) C APIs.
 *
 * FFRT is a task-based concurrent runtime library that automatically schedules
 * tasks according to their dependencies, eliminating the need for manual
 * thread management.
 *
 * @since 20
 */

/**
 * @file fiber.h
 *
 * @brief Declares the fiber interfaces in C.
 *
 * A fiber is a lightweight user-mode thread that enables efficient task scheduling
 * and context switching in user space.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 20
 */

#ifndef FFRT_API_C_FIBER_H
#define FFRT_API_C_FIBER_H

#include <stddef.h>
#include "type_def.h"

/**
 * @brief Initializes a fiber.
 *
 * This function initializes a fiber structure, preparing it for execution.
 * The caller is responsible for allocating the stack memory pointed to by
 * `stack` and keeping it valid for the entire lifetime of the fiber.
 *
 * @param fiber Indicates the pointer to the fiber structure to be initialized.
 * @param func Indicates the entry point function that the fiber will execute.
 * @param arg Indicates the argument to be passed to the entry point function.
 * @param stack Indicates the pointer to the memory region to be used as the fiber's stack.
 * @param stack_size Indicates the size of the stack in bytes. Must be large enough to hold the fiber context.
 * @return `ffrt_success` if the fiber is initialized;
 *         `ffrt_error_inval` if `stack_size` is too small to hold the fiber context.
 * @since 20
 */
FFRT_C_API int ffrt_fiber_init(ffrt_fiber_t* fiber, void(*func)(void*), void* arg, void* stack, size_t stack_size);

/**
 * @brief Switches execution context between two fibers.
 *
 * Switches the execution context by saving the current context into the fiber specified
 * by `from` and restoring the context from the fiber specified by `to`.
 *
 * Both `from` and `to` must point to fiber instances that have been initialized by
 * {@link ffrt_fiber_init}; otherwise the behavior is undefined.
 *
 * @param from Indicates the pointer to the fiber into which the current context will be saved.
 * @param to Indicates the pointer to the fiber from which the context will be restored.
 * @see ffrt_fiber_init
 * @since 20
 */
FFRT_C_API void ffrt_fiber_switch(ffrt_fiber_t* from, ffrt_fiber_t* to);

#endif // FFRT_API_C_FIBER_H
/** @} */
