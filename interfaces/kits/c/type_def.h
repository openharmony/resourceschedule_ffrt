/*
 * Copyright (c) 2023-2026 Huawei Device Co., Ltd.
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
 * @file type_def.h
 *
 * @brief Declares common types.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 10
 */

#ifndef FFRT_API_C_TYPE_DEF_H
#define FFRT_API_C_TYPE_DEF_H

#include <stdint.h>
#include <errno.h>

#ifdef __cplusplus
#define FFRT_C_API  extern "C"
#else
#define FFRT_C_API
#endif

/**
 * @brief Enumerates the task priority types used by concurrent queues to order task dispatch.
 *
 * @since 12
 */
typedef enum {
    /** Highest priority. Dispatched as soon as possible (handle time equals submission time); scheduled before high. */
    ffrt_queue_priority_immediate = 0,
    /** High priority. Sorted by handle time; scheduled before low. */
    ffrt_queue_priority_high,
    /** Low priority. Sorted by handle time; scheduled before idle. */
    ffrt_queue_priority_low,
    /** Lowest priority. Sorted by handle time; dispatched only when no other priority is present in the queue. */
    ffrt_queue_priority_idle,
} ffrt_queue_priority_t;

/**
 * @brief Enumerates the task QoS types.
 *
 * @since 10
 */
typedef enum {
    /**
     * @brief Inheritance.
     *
     * Inherits the QoS of the calling thread. Used when a task should adopt the priority of its creator.
     */
    ffrt_qos_inherit = -1,
    /**
     * @brief Background task.
     *
     * Lowest priority. Used for work the user is not aware of, such as background data sync or log flushing.
     */
    ffrt_qos_background,
    /**
     * @brief Utility-level task.
     *
     * Used for long-running tasks the user is aware of but does not actively wait on,
     * such as data loading or content indexing.
     */
    ffrt_qos_utility,
    /**
     * @brief Default type.
     *
     * Default QoS used when no specific priority is required; suitable for most general tasks.
     */
    ffrt_qos_default,
    /**
     * @brief User initiated.
     *
     * Used for tasks initiated by the user that need a quick response but do not block the UI,
     * such as opening a document or running a search.
     */
    ffrt_qos_user_initiated,
    /**
     * @brief Deadline request.
     *
     * Used for tasks with explicit deadlines. The system prioritizes scheduling resources for such tasks.
     *
     * @since 23
     */
    ffrt_qos_deadline_request,
    /**
     * @brief User interactive.
     *
     * Used for tasks that interact with the user, such as UI response.
     *
     * @since 23
     */
    ffrt_qos_user_interactive,
    /**
     * @brief Maximum QoS.
     *
     * Equivalent to ffrt_qos_user_interactive.
     *
     * @since 23
     */
    ffrt_qos_max = ffrt_qos_user_interactive,
} ffrt_qos_default_t;

/**
 * @brief Defines the QoS type used to set the QoS level of a task.
 *
 * @since 10
 */
typedef int ffrt_qos_t;

/**
 * @brief Defines the task function pointer type.
 *
 * The function pointer defines the entry point of an FFRT task. FFRT invokes
 * this function when the task is scheduled for execution, passing the user
 * data pointer through the single `void*` argument.
 *
 * @since 10
 */
typedef void (*ffrt_function_t)(void*);

/**
 * @brief Defines a task executor, used to define the task execution and destruction callbacks.
 *
 * The exec callback is invoked when the task is scheduled, and the destroy callback
 * is invoked after the task completes to release task-related resources. Together they
 * manage the full lifecycle of an FFRT task.
 *
 * @since 10
 */
typedef struct {
    /** Function used to execute a task. Called by the framework when the task is scheduled. */
    ffrt_function_t exec;
    /** Function used to destroy a task. Called by the framework after task execution to release resources. */
    ffrt_function_t destroy;
    /** Reserved field. Need to be set to 0. */
    uint64_t reserve[2];
} ffrt_function_header_t;

/**
 * @brief Defines the storage size of multiple types of structs, in bytes.
 *
 * @since 10
 */
typedef enum {
    /** Task attribute storage size, in bytes. */
    ffrt_task_attr_storage_size = 128,
    /** Task executor storage size, in bytes. */
    ffrt_auto_managed_function_storage_size = 64 + sizeof(ffrt_function_header_t),
    /** Mutex storage size, in bytes. */
    ffrt_mutex_storage_size = 64,
    /** Condition variable storage size, in bytes. */
    ffrt_cond_storage_size = 64,
    /** Queue storage size, in bytes. */
    ffrt_queue_attr_storage_size = 128,
    /**
     * @brief Rwlock storage size, in bytes.
     *
     * @since 18
     */
    ffrt_rwlock_storage_size = 64,
    /**
     * @brief Fiber storage size, in bytes.
     *
     * This constant defines the fiber storage size.
     * The actual value depends on the target architecture:
     * - __aarch64__: 22
     * - __arm__: 64
     * - __x86_64__: 8
     *
     * @since 20
     */
#if defined(__aarch64__)
    ffrt_fiber_storage_size = 22,
#elif defined(__arm__)
    ffrt_fiber_storage_size = 64,
#elif defined(__x86_64__)
    ffrt_fiber_storage_size = 8,
#else
#error "unsupported architecture"
#endif
} ffrt_storage_size_t;

/**
 * @brief Enumerates the task types, distinguishing general concurrent tasks from queue-scheduled tasks.
 *
 * @since 10
 */
typedef enum {
    /** General task. The task can be submitted to the FFRT scheduler and executed concurrently. */
    ffrt_function_kind_general,
    /** Queue task. The task is executed sequentially through a queue in submission order. */
    ffrt_function_kind_queue,
} ffrt_function_kind_t;

/**
 * @brief Enumerates the dependency types.
 *
 * Specifies how tasks depend on each other (data readiness or task completion).
 *
 * @since 10
 */
typedef enum {
    /** Data dependency type. The task is scheduled only after the referenced data is ready. */
    ffrt_dependence_data,
    /** Task dependency type. The task is scheduled only after the referenced task has completed. */
    ffrt_dependence_task,
} ffrt_dependence_type_t;

/**
 * @brief Defines the dependency data structure used to describe a single dependency between tasks.
 *
 * @since 10
 */
typedef struct {
    /** Dependency type. */
    ffrt_dependence_type_t type;
    /** Dependency pointer. Points to the data (data dependency) or task handle (task dependency). */
    const void* ptr;
} ffrt_dependence_t;

/**
 * @brief Defines the dependency structure, used to hold a list of dependencies for a task.
 *
 * @since 10
 */
typedef struct {
    /** Number of dependencies. */
    uint32_t len;
    /** Dependency data array. */
    const ffrt_dependence_t* items;
} ffrt_deps_t;

/**
 * @brief Defines the task attribute structure used to store task attribute information.
 *
 * @since 10
 */
typedef struct {
    /**
     * Internal storage backing the task attribute. Do not access directly; use the
     * {@link ffrt_task_attr_init} and `ffrt_task_attr_set_*` APIs to manage contents.
     */
    uint32_t storage[(ffrt_task_attr_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_task_attr_t;

/**
 * @brief Defines the queue attribute structure used to store queue attribute information.
 *
 * @since 10
 */
typedef struct {
    /**
     * Internal storage backing the queue attribute. Do not access directly; use the
     * {@link ffrt_queue_attr_init} and `ffrt_queue_attr_set_*` APIs to manage contents.
     */
    uint32_t storage[(ffrt_queue_attr_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_queue_attr_t;

/**
 * @brief Defines the task handle, which identifies different tasks.
 *
 * @since 10
 */
typedef void* ffrt_task_handle_t;

/**
 * @brief Enumerates the error codes returned by FFRT APIs.
 *
 * @since 10
 */
typedef enum {
    /** A generic error. */
    ffrt_error = -1,
    /** Success. */
    ffrt_success = 0,
    /** An out of memory error. */
    ffrt_error_nomem = ENOMEM,
    /** A timeout error. */
    ffrt_error_timedout = ETIMEDOUT,
    /** A busy error. The resource is busy, please retry later. */
    ffrt_error_busy = EBUSY,
    /** An invalid value error. */
    ffrt_error_inval = EINVAL
} ffrt_error_t;

/**
 * @brief Defines the condition variable attribute structure used to store condition variable attribute information.
 *
 * @since 10
 */
typedef struct {
    /** Internal storage backing the condition variable attribute. Do not access directly. */
    long storage;
} ffrt_condattr_t;

/**
 * @brief Defines the mutex attribute structure used to store mutex attribute information.
 *
 * @since 10
 */
typedef struct {
    /**
     * Internal storage backing the mutex attribute. Do not access directly;
     * use {@link ffrt_mutexattr_init} to initialize.
     */
    long storage;
} ffrt_mutexattr_t;

/**
 * @brief Defines the rwlock attribute structure used to store rwlock attribute information.
 *
 * @since 18
 */
typedef struct {
    /**
     * Internal storage backing the rwlock attribute. Do not access directly; direct access may
     * cause the rwlock attribute to become invalid.
     */
    long storage;
} ffrt_rwlockattr_t;

/**
 * @brief Enumerates the mutex types.
 *
 * @since 12
 */
typedef enum {
    /** Normal mutex type. */
    ffrt_mutex_normal = 0,
    /** Recursive mutex type, which allows the same thread to lock the mutex multiple times. */
    ffrt_mutex_recursive = 2,
    /** Default mutex type, equivalent to ffrt_mutex_normal. */
    ffrt_mutex_default = ffrt_mutex_normal
} ffrt_mutex_type;

/**
 * @brief Defines the mutex structure used to store internal data of the mutex.
 *
 * @since 10
 */
typedef struct {
    /** Internal storage backing the mutex. Do not access directly; use the `ffrt_mutex_*` APIs. */
    uint32_t storage[(ffrt_mutex_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_mutex_t;

/**
 * @brief Defines the rwlock structure used to store internal data of the rwlock.
 *
 * @since 18
 */
typedef struct {
    /** Internal storage backing the rwlock. Do not access directly; use the `ffrt_rwlock_*` APIs. */
    uint32_t storage[(ffrt_rwlock_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_rwlock_t;

/**
 * @brief Defines the condition variable structure used to store internal data of the condition variable.
 *
 * @since 10
 */
typedef struct {
    /** Internal storage backing the condition variable. Do not access directly; use the `ffrt_cond_*` APIs. */
    uint32_t storage[(ffrt_cond_storage_size + sizeof(uint32_t) - 1) / sizeof(uint32_t)];
} ffrt_cond_t;

/**
 * @brief Defines the fiber structure used to store fiber execution context.
 *
 * @since 20
 */
typedef struct {
    /**
     * Internal storage backing the fiber execution context. Do not access directly;
     * use {@link ffrt_fiber_init} to initialize and {@link ffrt_fiber_switch} to switch.
     */
    uintptr_t storage[ffrt_fiber_storage_size];
#ifdef TSAN_MODE
    void* tsanFiber = nullptr;
#endif
} ffrt_fiber_t;

/**
 * @brief Defines the poller callback function type.
 *
 * The callback is invoked when the poller detects a registered event. The data
 * pointer carries user data passed in at registration time, and the event value
 * identifies the triggered event type.
 *
 * @param data Indicates the user data pointer passed in at poller registration.
 * @param event Indicates the event type that triggered the callback.
 * @since 12
 */
typedef void (*ffrt_poller_cb)(void* data, uint32_t event);

/**
 * @brief Defines the timer callback function type.
 *
 * The callback is invoked when the timer expires. The data pointer carries
 * user data passed in at timer registration.
 *
 * @param data Indicates the user data pointer passed in at timer registration.
 * @since 12
 */
typedef void (*ffrt_timer_cb)(void* data);

/**
 * @brief Defines the timer handle used to identify a created timer.
 *
 * @since 12
 */
typedef int ffrt_timer_t;

#ifdef __cplusplus
namespace ffrt {

/**
 * @brief Enumerates the task QoS types.
 *
 * Each enumerator mirrors the corresponding enumerator in {@link ffrt_qos_default_t}.
 *
 * @since 10
 */
enum qos_default {
    /**
     * @brief Inheritance.
     *
     * Inherits the QoS of the calling thread. Used when a task should adopt the priority of its creator.
     */
    qos_inherit = ffrt_qos_inherit,
    /**
     * @brief Background task.
     *
     * Lowest priority. Used for work the user is not aware of, such as background data sync or log flushing.
     */
    qos_background = ffrt_qos_background,
    /**
     * @brief Utility-level task.
     *
     * Used for long-running tasks the user is aware of but does not actively wait on,
     * such as data loading or content indexing.
     */
    qos_utility = ffrt_qos_utility,
    /**
     * @brief Default type.
     *
     * Default QoS used when no specific priority is required; suitable for most general tasks.
     */
    qos_default = ffrt_qos_default,
    /**
     * @brief User initiated.
     *
     * Used for tasks initiated by the user that need a quick response but do not block the UI,
     * such as opening a document or running a search.
     */
    qos_user_initiated = ffrt_qos_user_initiated,
    /**
     * @brief Deadline request.
     *
     * Used for tasks with explicit deadlines. The system prioritizes scheduling resources for such tasks.
     *
     * @since 23
     */
    qos_deadline_request = ffrt_qos_deadline_request,
    /**
     * @brief User interactive.
     *
     * Used for tasks that interact with the user, such as UI response.
     *
     * @since 23
     */
    qos_user_interactive = ffrt_qos_user_interactive,
    /**
     * @brief Maximum QoS.
     *
     * Equivalent to ffrt_qos_user_interactive.
     *
     * @since 23
     */
    qos_max = ffrt_qos_user_interactive,
};

/**
 * @brief Defines the QoS type.
 *
 * @since 10
 */
using qos = int;

}

#endif // __cplusplus
#endif // FFRT_API_C_TYPE_DEF_H
/** @} */
