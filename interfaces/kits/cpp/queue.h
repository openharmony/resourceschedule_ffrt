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
 * @file queue.h
 *
 * @brief Declares the queue interfaces in C++.
 *
 * @since 10
 * @version 1.0
 */
#ifndef FFRT_API_CPP_QUEUE_H
#define FFRT_API_CPP_QUEUE_H

#include "c/queue.h"
#include "task.h"

namespace ffrt {
enum queue_type {
    queue_serial = ffrt_queue_serial,
    queue_concurrent = ffrt_queue_concurrent,
    queue_max = ffrt_queue_max,
};
class queue_attr : public ffrt_queue_attr_t {
public:
    queue_attr()
    {
        ffrt_queue_attr_init(this);
    }

    ~queue_attr()
    {
        ffrt_queue_attr_destroy(this);
    }

    queue_attr(const queue_attr&) = delete;
    queue_attr& operator=(const queue_attr&) = delete;

    /**
     * @brief Sets the QoS for this queue attribute.
     *
     * @param attr Indicates the QoS.
     * @since 10
     * @version 1.0
     */
    inline queue_attr& qos(qos qos_)
    {
        ffrt_queue_attr_set_qos(this, qos_);
        return *this;
    }

    // get qos
    inline int qos() const
    {
        return ffrt_queue_attr_get_qos(this);
    }

    // set timeout
    inline queue_attr& timeout(uint64_t timeout_us)
    {
        ffrt_queue_attr_set_timeout(this, timeout_us);
        return *this;
    }

    // get timeout
    inline uint64_t timeout() const
    {
        return ffrt_queue_attr_get_timeout(this);
    }

    // set timeout callback
    inline queue_attr& callback(const std::function<void()>& func)
    {
        ffrt_queue_attr_set_callback(this, create_function_wrapper(func, ffrt_function_kind_queue));
        return *this;
    }

    // get timeout callback
    inline ffrt_function_header_t* callback() const
    {
        return ffrt_queue_attr_get_callback(this);
    }

    // set max concurrency of queue
    inline queue_attr& max_concurrency(const int max_concurrency)
    {
        ffrt_queue_attr_set_max_concurrency(this, max_concurrency);
        return *this;
    }

    // get max concurrency of queue
    inline int max_concurrency() const
    {
        return ffrt_queue_attr_get_max_concurrency(this);
    }
};

class queue {
public:
    queue(const queue_type type, const char* name, const queue_attr& attr = {})
    {
        queue_handle = ffrt_queue_create(ffrt_queue_type_t(type), name, &attr);
    }

    queue(const char* name, const queue_attr& attr = {})
    {
        queue_handle = ffrt_queue_create(ffrt_queue_serial, name, &attr);
    }

    ~queue()
    {
        ffrt_queue_destroy(queue_handle);
    }

    queue(queue const&) = delete;
    void operator=(queue const&) = delete;

    /**
     * @brief Submits a task to this queue.
     *
     * @param func Indicates a task executor function closure.
     * @since 10
     * @version 1.0
     */
    inline void submit(const std::function<void()>& func)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), nullptr);
    }

    /**
     * @brief Submits a task with a specified attribute to this queue.
     *
     * @param func Indicates a task executor function closure.
     * @param attr Indicates a task attribute.
     * @since 10
     * @version 1.0
     */
    inline void submit(const std::function<void()>& func, const task_attr& attr)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), &attr);
    }

    /**
     * @brief Submits a task to this queue.
     *
     * @param func Indicates a task executor function closure.
     * @since 10
     * @version 1.0
     */
    inline void submit(std::function<void()>&& func)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), nullptr);
    }

    /**
     * @brief Submits a task with a specified attribute to this queue.
     *
     * @param func Indicates a task executor function closure.
     * @param attr Indicates a task attribute.
     * @since 10
     * @version 1.0
     */
    inline void submit(std::function<void()>&& func, const task_attr& attr)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), &attr);
    }

    /**
     * @brief Submits a task to this queue, and obtains a task handle.
     *
     * @param func Indicates a task executor function closure.
     * @return Returns a non-null task handle if the task is submitted;
               returns a null pointer otherwise.
     * @since 10
     * @version 1.0
     */
    inline task_handle submit_h(const std::function<void()>& func)
    {
        return ffrt_queue_submit_h(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), nullptr);
    }

    /**
     * @brief Submits a task with a specified attribute to this queue, and obtains a task handle.
     *
     * @param func Indicates a task executor function closure.
     * @param attr Indicates a task attribute.
     * @return Returns a non-null task handle if the task is submitted;
               returns a null pointer otherwise.
     * @since 10
     * @version 1.0
     */
    inline task_handle submit_h(const std::function<void()>& func, const task_attr& attr)
    {
        return ffrt_queue_submit_h(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), &attr);
    }

    /**
     * @brief Submits a task to this queue, and obtains a task handle.
     *
     * @param func Indicates a task executor function closure.
     * @return Returns a non-null task handle if the task is submitted;
               returns a null pointer otherwise.
     * @since 10
     * @version 1.0
     */
    inline task_handle submit_h(std::function<void()>&& func)
    {
        return ffrt_queue_submit_h(
            queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), nullptr);
    }

    /**
     * @brief Submits a task with a specified attribute to this queue, and obtains a task handle.
     *
     * @param func Indicates a task executor function closure.
     * @param attr Indicates a task attribute.
     * @return Returns a non-null task handle if the task is submitted;
               returns a null pointer otherwise.
     * @since 10
     * @version 1.0
     */
    inline task_handle submit_h(std::function<void()>&& func, const task_attr& attr)
    {
        return ffrt_queue_submit_h(
            queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), &attr);
    }

    /**
     * @brief Cancels a task.
     *
     * @param handle Indicates a task handle.
     * @return Returns <b>0</b> if the task is canceled;
               returns <b>-1</b> otherwise.
     * @since 10
     * @version 1.0
     */
    inline int cancel(const task_handle& handle)
    {
        return ffrt_queue_cancel(handle);
    }

    /**
     * @brief Waits until a task is complete.
     *
     * @param handle Indicates a task handle.
     * @since 10
     * @version 1.0
     */
    inline void wait(const task_handle& handle)
    {
        return ffrt_queue_wait(handle);
    }

private:
    ffrt_queue_t queue_handle = nullptr;
};
} // namespace ffrt

#endif // FFRT_API_CPP_QUEUE_H