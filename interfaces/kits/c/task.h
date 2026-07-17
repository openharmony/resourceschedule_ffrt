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
 * @since 10
 */

/**
 * @file task.h
 *
 * @brief Declares the FFRT task C APIs, including task attribute initialization and destruction,
 * task QoS configuration, task delay time management, concurrent queue task priority management,
 * task stack size management, task submission and scheduling, task handle reference counting,
 * and task wait operations.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 10
 */

#ifndef FFRT_API_C_TASK_H
#define FFRT_API_C_TASK_H

#include <stdint.h>
#include <stddef.h>
#include "type_def.h"

/**
 * @brief Initializes a task attribute.
 *
 * After the call, the task attribute is set to its default values (for example, the QoS
 * defaults to {@link ffrt_qos_default}). The caller is expected to invoke
 * {@link ffrt_task_attr_destroy} to release the attribute when it is no longer needed.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return `0` if the task attribute is initialized;
 *         `-1` otherwise.
 * @since 10
 */
FFRT_C_API int ffrt_task_attr_init(ffrt_task_attr_t* attr);

/**
 * @brief Sets the name of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param name Indicates a pointer to the task name.
 * @since 10
 */
FFRT_C_API void ffrt_task_attr_set_name(ffrt_task_attr_t* attr, const char* name);

/**
 * @brief Gets the name of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return A non-null pointer to the task name if the name is obtained;
 *         a null pointer otherwise.
 * @since 10
 */
FFRT_C_API const char* ffrt_task_attr_get_name(const ffrt_task_attr_t* attr);

/**
 * @brief Destroys a task attribute.
 *
 * This interface must be called on a task attribute that was previously initialized with
 * {@link ffrt_task_attr_init}, and is used to release the resources held by the attribute.
 * The attribute must not be used again after destruction.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @since 10
 */
FFRT_C_API void ffrt_task_attr_destroy(ffrt_task_attr_t* attr);

/**
 * @brief Sets the QoS of a task attribute.
 *
 * The QoS controls the scheduling priority of the task. For example, assign a
 * higher QoS to user-facing work to keep the response time low, and a lower QoS to
 * background or housekeeping work to reduce its impact on system resources.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param qos Indicates the QoS level to set. The available levels are defined by {@link ffrt_qos_t}.
 * @since 10
 */
FFRT_C_API void ffrt_task_attr_set_qos(ffrt_task_attr_t* attr, ffrt_qos_t qos);

/**
 * @brief Gets the QoS of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return The QoS, which is `ffrt_qos_default` by default.
 * @since 10
 */
FFRT_C_API ffrt_qos_t ffrt_task_attr_get_qos(const ffrt_task_attr_t* attr);

/**
 * @brief Sets the delay time of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param delay_us Indicates the delay time, in microseconds.
 * @since 10
 */
FFRT_C_API void ffrt_task_attr_set_delay(ffrt_task_attr_t* attr, uint64_t delay_us);

/**
 * @brief Gets the delay time of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return The delay time, in microseconds.
 * @since 10
 */
FFRT_C_API uint64_t ffrt_task_attr_get_delay(const ffrt_task_attr_t* attr);

/**
 * @brief Sets the priority of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param priority Indicates the priority of a concurrent queue task.
 *                 The available priorities are defined by {@link ffrt_queue_priority_t}; higher priorities
 *                 are scheduled before lower priorities within the same concurrent queue. Values outside
 *                 the valid range are silently ignored.
 * @since 12
 */
FFRT_C_API void ffrt_task_attr_set_queue_priority(ffrt_task_attr_t* attr, ffrt_queue_priority_t priority);

/**
 * @brief Gets the priority of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return The priority of a concurrent queue task.
 * @since 12
 */
FFRT_C_API ffrt_queue_priority_t ffrt_task_attr_get_queue_priority(const ffrt_task_attr_t* attr);

/**
 * @brief Sets the stack size of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param size Indicates the task stack size, in bytes. The value must be greater than the
 *             minimum stack size supported by the system, or stack overflow may occur. Setting it too
 *             large may result in memory allocation failure.
 * @since 12
 */
FFRT_C_API void ffrt_task_attr_set_stack_size(ffrt_task_attr_t* attr, uint64_t size);

/**
 * @brief Gets the stack size of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return The task stack size, in bytes.
 * @since 12
 */
FFRT_C_API uint64_t ffrt_task_attr_get_stack_size(const ffrt_task_attr_t* attr);

/**
 * @brief Sets the schedule timeout of a task attribute.
 *
 * The lower limit of the timeout value is 100 ms; values below 100 ms are clamped to 100 ms.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param timeout_us Indicates the task schedule timeout, in microseconds.
 * @see ffrt_task_attr_set_timeout_callback
 */
FFRT_C_API void ffrt_task_attr_set_timeout(ffrt_task_attr_t* attr, uint64_t timeout_us);

/**
 * @brief Gets the schedule timeout of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return The task schedule timeout, in microseconds.
 * @see ffrt_task_attr_set_timeout
 */
FFRT_C_API uint64_t ffrt_task_attr_get_timeout(const ffrt_task_attr_t* attr);

/**
 * @brief Sets the timeout callback function of a task attribute.
 *
 * The callback is triggered when a task cannot be scheduled before the timeout set by
 * {@link ffrt_task_attr_set_timeout}.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @param f Indicates a pointer to the function to execute after a scheduling timeout.
 * @see ffrt_task_attr_set_timeout
 */
FFRT_C_API void ffrt_task_attr_set_timeout_callback(ffrt_task_attr_t* attr, ffrt_function_header_t* f);

/**
 * @brief Gets the timeout callback function of a task attribute.
 *
 * @param attr Indicates a pointer to the task attribute.
 * @return The task scheduling timeout callback function.
 * @see ffrt_task_attr_set_timeout_callback
 */
FFRT_C_API ffrt_function_header_t* ffrt_task_attr_get_timeout_callback(const ffrt_task_attr_t* attr);

/**
 * @brief Updates the QoS of this task.
 *
 * Use this interface to adjust the scheduling priority of the currently running task
 * when its priority needs to change during execution, for example when a background
 * task starts to handle a user-initiated operation and requires faster response.
 *
 * @param qos Indicates the new QoS level for this task. The available levels are defined by {@link ffrt_qos_t}.
 * @return `0` if the QoS is updated, or if the new QoS is the same as the current QoS;
 *         `1` if the QoS map is not registered, the current task is null, or the
 *         current task is not a general-type task (i.e., not submitted through
 *         {@link ffrt_submit_base} or {@link ffrt_submit_h_base}).
 * @see ffrt_this_task_get_qos
 * @since 10
 */
FFRT_C_API int ffrt_this_task_update_qos(ffrt_qos_t qos);

/**
 * @brief Gets the QoS of this task.
 *
 * @return The task QoS.
 * @since 12
 */
FFRT_C_API ffrt_qos_t ffrt_this_task_get_qos(void);

/**
 * @brief Gets the ID of this task.
 *
 * @return The unique task ID of the currently running task.
 * @since 10
 */
FFRT_C_API uint64_t ffrt_this_task_get_id(void);

/**
 * @brief Allocates memory for the function execution structure.
 *
 * The allocated memory is used as the task executor wrapper passed to
 * {@link ffrt_submit_base} or {@link ffrt_submit_h_base} when submitting a task.
 * The memory is automatically released by the FFRT runtime after the submitted task
 * finishes execution, so the caller does not need to free it manually.
 *
 * @param kind Indicates the type of the function execution structure.
 *             Use a common (general) kind for tasks submitted through {@link ffrt_submit_base} or
 *             {@link ffrt_submit_h_base}, and a queue kind for tasks submitted through the concurrent
 *             queue submit interface.
 * @return A non-null pointer if the memory is allocated;
 *         a null pointer otherwise.
 * @see ffrt_submit_base
 * @see ffrt_submit_h_base
 * @since 10
 */
FFRT_C_API void* ffrt_alloc_auto_managed_function_storage_base(ffrt_function_kind_t kind);

/**
 * @brief Submits a task.
 *
 * The task is submitted to the FFRT scheduler together with its input and output dependencies
 * and the task attribute. The scheduler uses the dependencies and the task QoS to determine
 * when the task becomes ready to run and which worker executes it. This is the underlying
 * submission interface; the simplified wrapper {@link ffrt_submit_f} can be used when no
 * task destroy callback is required. Unlike {@link ffrt_submit_h_base}, this interface
 * does not return a task handle and should be used when the caller does not need to track
 * the task after submission.
 *
 * If a task delay has been set on the attribute with {@link ffrt_task_attr_set_delay}, the
 * input and output dependencies are ignored and the task is scheduled after the delay elapses.
 *
 * @param f Indicates a pointer to the task executor wrapper. The wrapper must be allocated
 *          with {@link ffrt_alloc_auto_managed_function_storage_base} and must include a task destroy callback.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a pointer to the task attribute.
 * @see ffrt_submit_h_base
 * @since 10
 */
FFRT_C_API void ffrt_submit_base(ffrt_function_header_t* f, const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps,
    const ffrt_task_attr_t* attr);

/**
 * @brief Submits a task, and obtains a task handle.
 *
 * The task is submitted to the FFRT scheduler together with its input and output dependencies
 * and the task attribute. The scheduler uses the dependencies to determine when the task
 * becomes ready to run. The returned handle can be used with {@link ffrt_wait_deps} to wait
 * for the task, or passed as an input dependency to other submitted tasks to build a
 * dependency chain. This is the underlying submission interface that returns a task handle;
 * the simplified wrapper {@link ffrt_submit_h_f} can be used when no task destroy callback
 * is required. The returned handle should be released with {@link ffrt_task_handle_destroy}
 * when it is no longer needed, and its reference count can be managed with
 * {@link ffrt_task_handle_inc_ref} and {@link ffrt_task_handle_dec_ref}.
 *
 * @param f Indicates a pointer to the task executor wrapper. The wrapper must be allocated
 *          with {@link ffrt_alloc_auto_managed_function_storage_base} and must include a task destroy callback.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a pointer to the task attribute.
 * @return A non-null task handle if the task is submitted;
 *         a null pointer otherwise.
 * @see ffrt_submit_base
 * @since 10
 */
FFRT_C_API ffrt_task_handle_t ffrt_submit_h_base(ffrt_function_header_t* f, const ffrt_deps_t* in_deps,
    const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr);

/**
 * @brief Submits a task, simplified from the {@link ffrt_submit_base} interface.
 *
 * This interface wraps the provided task function and its argument into a task wrapper
 * designated as a general task (`ffrt_function_kind_general`). During wrapper creation, the
 * task destroy callback (after_func), which is intended to handle any post-execution cleanup,
 * is set to NULL, thus omitting any additional cleanup actions. The resulting task wrapper is
 * then submitted using the underlying {@link ffrt_submit_base} interface.
 *
 * If a task delay has been set on the attribute with {@link ffrt_task_attr_set_delay}, the
 * input and output dependencies are ignored and the task is scheduled after the delay elapses.
 *
 * @param func Indicates a task function to be executed.
 * @param arg Indicates a pointer to the argument or closure data that will be passed to the task function.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a pointer to the task attribute.
 * @see ffrt_submit_base
 * @since 20
 */
FFRT_C_API void ffrt_submit_f(ffrt_function_t func, void* arg, const ffrt_deps_t* in_deps, const ffrt_deps_t* out_deps,
    const ffrt_task_attr_t* attr);

/**
 * @brief Submits a task, and obtains a handle, simplified from the {@link ffrt_submit_h_base} interface.
 *
 * This interface wraps the provided task function and its argument into a task wrapper
 * designated as a general task (`ffrt_function_kind_general`). During wrapper creation, the
 * task destroy callback (after_func), which is intended to handle any post-execution cleanup,
 * is set to NULL, thus omitting any additional cleanup actions. The resulting task wrapper is
 * then submitted using the underlying {@link ffrt_submit_h_base} interface.
 *
 * If a task delay has been set on the attribute with {@link ffrt_task_attr_set_delay}, the
 * input and output dependencies are ignored and the task is scheduled after the delay elapses.
 * The returned task handle should be released with {@link ffrt_task_handle_destroy} when it
 * is no longer needed.
 *
 * @param func Indicates a task function to be executed.
 * @param arg Indicates a pointer to the argument or closure data that will be passed to the task function.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a pointer to the task attribute.
 * @return A non-null task handle if the task is submitted;
 *         a null pointer otherwise.
 * @see ffrt_submit_h_base
 * @since 20
 */
FFRT_C_API ffrt_task_handle_t ffrt_submit_h_f(ffrt_function_t func, void* arg, const ffrt_deps_t* in_deps,
    const ffrt_deps_t* out_deps, const ffrt_task_attr_t* attr);

/**
 * @brief Increases the reference count of a task handle.
 *
 * The reference count of the task handle is incremented by one, and the value of the
 * reference count before the increment is returned.
 *
 * @param handle Indicates a task handle, obtained from {@link ffrt_submit_h_base} or {@link ffrt_submit_h_f}.
 * @return The task handle reference count value before the increment;
 *         `UINT_MAX` if `handle` is null.
 * @since 12
 */
FFRT_C_API uint32_t ffrt_task_handle_inc_ref(ffrt_task_handle_t handle);

/**
 * @brief Decreases the reference count of a task handle.
 *
 * The reference count of the task handle is decremented by one, and the value of the
 * reference count before the decrement is returned. Pair this call with
 * {@link ffrt_task_handle_inc_ref} and use {@link ffrt_task_handle_destroy} to release
 * the handle when it is no longer needed.
 *
 * @param handle Indicates a task handle.
 * @return The task handle reference count value before the decrement;
 *         `UINT_MAX` if `handle` is null.
 * @since 12
 */
FFRT_C_API uint32_t ffrt_task_handle_dec_ref(ffrt_task_handle_t handle);

/**
 * @brief Destroys a task handle.
 *
 * After the call, the task handle is destroyed and the resources associated with it are
 * released. The handle must not be used again after destruction.
 *
 * @param handle Indicates a task handle.
 * @since 10
 */
FFRT_C_API void ffrt_task_handle_destroy(ffrt_task_handle_t handle);

/**
 * @brief Waits until the dependent tasks are complete.
 *
 * @param deps Indicates a pointer to the list of dependent tasks. The calling task is
 *             blocked until all tasks referenced by this dependency list have finished executing.
 * @since 10
 */
FFRT_C_API void ffrt_wait_deps(const ffrt_deps_t* deps);

/**
 * @brief Waits until all submitted tasks are complete.
 *
 * @since 10
 */
FFRT_C_API void ffrt_wait(void);

/**
 * @brief Sets the worker thread stack size of a specified QoS level.
 *
 * @param qos Indicates the QoS level to configure. The available levels are defined by {@link ffrt_qos_t}.
 * @param stack_size Indicates the worker thread stack size, in bytes. The value must be greater than the
 *                   minimum stack size supported by the system, or stack overflow may occur. Setting it
 *                   too large may result in memory allocation failure.
 * @return `ffrt_success` if the stack size is set;
 *         `ffrt_error_inval` if `qos` or `stack_size` is invalid;
 *         `ffrt_error` if the worker stack size cannot be updated.
 */
FFRT_C_API ffrt_error_t ffrt_set_worker_stack_size(ffrt_qos_t qos, size_t stack_size);

/**
 * @brief Gets the ID of the task identified by a handle.
 *
 * @param handle Indicates a task handle.
 * @return The ID of the task identified by the handle.
 * @see ffrt_this_task_get_id
 */
FFRT_C_API uint64_t ffrt_task_handle_get_id(ffrt_task_handle_t handle);

#endif // FFRT_API_C_TASK_H
/** @} */
