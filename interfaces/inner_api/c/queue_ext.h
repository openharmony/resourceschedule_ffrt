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

#ifndef FFRT_INNER_API_C_QUEUE_H
#define FFRT_INNER_API_C_QUEUE_H

#include <stdbool.h>
#include "c/queue.h"

typedef enum {
    ffrt_queue_eventhandler_interactive = 3,
    ffrt_queue_eventhandler_adapter = 4,
    ffrt_queue_inner_max,
} ffrt_inner_queue_type_t;

typedef enum {
    /** highest priority, should be distributed until the tasks in the queue are completed */
    ffrt_inner_queue_priority_vip = 0,
    /** should be distributed at once if possible, handle time equals to send time, prior to high level */
    ffrt_inner_queue_priority_immediate,
    /** high priority, sorted by handle time, prior to low level. */
    ffrt_inner_queue_priority_high,
    /** low priority, sorted by handle time, prior to idle level. */
    ffrt_inner_queue_priority_low,
    /** lowest priority, sorted by handle time, only distribute when there is no other level inside queue. */
    ffrt_inner_queue_priority_idle,
} ffrt_inner_queue_priority_t;

/**
 * @brief Submits a task to a queue, for tasks with the same delay, insert the header.
 *
 * @param queue Indicates a queue handle.
 * @param f Indicates a pointer to the task executor.
 * @param attr Indicates a pointer to the task attribute.
 * @version 1.0
 */
FFRT_C_API void ffrt_queue_submit_head(ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr);

/**
 * @brief Submits a task to the queue, and obtains a task handle, for tasks with the same delay, insert the header.
 *
 * @param queue Indicates a queue handle.
 * @param f Indicates a pointer to the task executor.
 * @param attr Indicates a pointer to the task attribute.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @version 1.0
 */
FFRT_C_API ffrt_task_handle_t ffrt_queue_submit_head_h(
    ffrt_queue_t queue, ffrt_function_header_t* f, const ffrt_task_attr_t* attr);

/**
 * @brief Checks whether a task with the given name can be found in the queue.
 *
 * @param queue Indicates a queue handle.
 * @param name Indicates name to be searched for, regular expressions are supported.
 * @return Returns whether the task is found.
 * @version 1.0
 */
FFRT_C_API bool ffrt_queue_has_task(ffrt_queue_t queue, const char* name);

/**
 * @brief Cancels all unexecuted tasks in the queue.
 *
 * @param queue Indicates a queue handle.
 * @version 1.0
 */
FFRT_C_API void ffrt_queue_cancel_all(ffrt_queue_t queue);

/**
 * @brief Cancels a task with the given name in the queue.
 *
 * @param queue Indicates a queue handle.
 * @param name Indicates name of the task to be canceled, regular expressions are supperted.
 * @return Returns <b>0</b> if the task is canceled;
           returns <b>1</b> otherwise.
 * @version 1.0
 */
FFRT_C_API int ffrt_queue_cancel_by_name(ffrt_queue_t queue, const char* name);

/**
 * @brief Checks whether the queue is idle.
 *
 * @param queue Indicates a queue handle.
 * @return Returns whether the queue is idle.
 * @version 1.0
 */
FFRT_C_API bool ffrt_queue_is_idle(ffrt_queue_t queue);

/**
 * @brief Dumps queue information;
 *        including current execution, historical execution, and remaining unexecuted task information, etc.
 *
 * @param queue Indicates a queue handle.
 * @param tag Indicates tag prefix for dump information.
 * @param buf Indicates produce output, write to the character string buf.
 * @param len Indicates the size of the buffer (in bytes).
 * @param history_info Indicates whether dump history information.
 * @return Returns the number of characters printed (not including the terminating null byte '\0');
           returns -1 if an error occurred, pay special attention to returning -1 when truncation occurs.
 * @version 1.0
 */
FFRT_C_API int ffrt_queue_dump(ffrt_queue_t queue, const char* tag, char* buf, uint32_t len, bool history_info);

/**
 * @brief Dumps queue task count with specified priority.
 *
 * @param queue Indicates a queue handle.
 * @param priority Indicates the execute priority of queue task.
 * @return Returns the count of tasks;
           returns -1 if an error occurred.
 * @version 1.0
 */
FFRT_C_API int ffrt_queue_size_dump(ffrt_queue_t queue, ffrt_inner_queue_priority_t priority);

/**
 * @brief Binds an eventhandler object to the queue.
 *
 * @param queue Indicates a queue handle.
 * @param eventhandler Indicates an eventhandler pointer.
 * @version 1.0
 */
FFRT_C_API void ffrt_queue_set_eventhandler(ffrt_queue_t queue, void* eventhandler);

/**
 * @brief Obtains the handler bound to the queue that is being executed on the current worker.
 *
 * @return Returns a non-null eventhandler pointer;
           returns a null pointer if the current task is not bound to an eventhandler.
 * @version 1.0
 */
FFRT_C_API void* ffrt_get_current_queue_eventhandler();

#endif