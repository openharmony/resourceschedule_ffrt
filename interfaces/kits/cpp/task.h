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
 * @file task.h
 *
 * @brief Declares the task interfaces in C++.
 *
 * @since 10
 * @version 1.0
 */
#ifndef FFRT_API_CPP_TASK_H
#define FFRT_API_CPP_TASK_H
#include <vector>
#include <functional>
#include "c/task.h"

namespace ffrt {
class task_attr : public ffrt_task_attr_t {
public:
    task_attr()
    {
        ffrt_task_attr_init(this);
    }

    ~task_attr()
    {
        ffrt_task_attr_destroy(this);
    }

    task_attr(const task_attr&) = delete;
    task_attr& operator=(const task_attr&) = delete;

    /**
     * @brief Sets a task name.
     *
     * @param name Indicates a pointer to the task name.
     * @since 10
     * @version 1.0
     */
    inline task_attr& name(const char* name)
    {
        ffrt_task_attr_set_name(this, name);
        return *this;
    }

    /**
     * @brief Obtains the task name.
     *
     * @return Returns a pointer to the task name.
     * @since 10
     * @version 1.0
     */
    inline const char* name() const
    {
        return ffrt_task_attr_get_name(this);
    }

    /**
     * @brief Sets the QoS for this task.
     *
     * @param qos Indicates the QoS.
     * @since 10
     * @version 1.0
     */
    inline task_attr& qos(qos qos_)
    {
        ffrt_task_attr_set_qos(this, qos_);
        return *this;
    }

    /**
     * @brief Obtains the QoS of this task.
     *
     * @return Returns the QoS.
     * @since 10
     * @version 1.0
     */
    inline int qos() const
    {
        return ffrt_task_attr_get_qos(this);
    }

    /**
     * @brief Sets the delay time for this task.
     *
     * @param delay_us Indicates the delay time, in microseconds.
     * @since 10
     * @version 1.0
     */
    inline task_attr& delay(uint64_t delay_us)
    {
        ffrt_task_attr_set_delay(this, delay_us);
        return *this;
    }

    /**
     * @brief Obtains the delay time of this task.
     *
     * @return Returns the delay time.
     * @since 10
     * @version 1.0
     */
    inline uint64_t delay() const
    {
        return ffrt_task_attr_get_delay(this);
    }

    /**
     * @brief Sets the priority for this task.
     *
     * @param priority Indicates the execute priority of concurrent queue task.
     * @since 12
     * @version 1.0
     */
    inline task_attr& priority(ffrt_queue_priority_t prio)
    {
        ffrt_task_attr_set_queue_priority(this, prio);
        return *this;
    }

    /**
     * @brief Obtains the priority of this task.
     *
     * @return Returns the priority of concurrent queue task.
     * @since 12
     * @version 1.0
     */
    inline ffrt_queue_priority_t priority() const
    {
        return ffrt_task_attr_get_queue_priority(this);
    }
};

class task_handle {
public:
    task_handle() : p(nullptr)
    {
    }
    task_handle(ffrt_task_handle_t p) : p(p)
    {
    }

    ~task_handle()
    {
        if (p) {
            ffrt_task_handle_destroy(p);
        }
    }

    task_handle(task_handle const&) = delete;
    task_handle& operator=(task_handle const&) = delete;

    inline task_handle(task_handle&& h)
    {
        *this = std::move(h);
    }

    inline task_handle& operator=(task_handle&& h)
    {
        if (this != &h) {
            if (p) {
                ffrt_task_handle_destroy(p);
            }
            p = h.p;
            h.p = nullptr;
        }
        return *this;
    }

    inline operator void* () const
    {
        return p;
    }

private:
    ffrt_task_handle_t p = nullptr;
};

struct dependence : ffrt_dependence_t {
    dependence(const void* d)
    {
        type = ffrt_dependence_data;
        ptr = d;
    }
    dependence(const task_handle& h)
    {
        type = ffrt_dependence_task;
        ptr = h;
    }
};

template<class T>
struct function {
    ffrt_function_header_t header;
    T closure;
};

template<class T>
void exec_function_wrapper(void* t)
{
    auto f = reinterpret_cast<function<std::decay_t<T>>*>(t);
    f->closure();
}

template<class T>
void destroy_function_wrapper(void* t)
{
    auto f = reinterpret_cast<function<std::decay_t<T>>*>(t);
    f->closure = nullptr;
}

template<class T>
inline ffrt_function_header_t* create_function_wrapper(T&& func,
    ffrt_function_kind_t kind = ffrt_function_kind_general)
{
    using function_type = function<std::decay_t<T>>;
    static_assert(sizeof(function_type) <= ffrt_auto_managed_function_storage_size,
        "size of function must be less than ffrt_auto_managed_function_storage_size");

    auto p = ffrt_alloc_auto_managed_function_storage_base(kind);
    auto f = new (p)function_type;
    f->header.exec = exec_function_wrapper<T>;
    f->header.destroy = destroy_function_wrapper<T>;
    f->closure = std::forward<T>(func);
    return reinterpret_cast<ffrt_function_header_t*>(f);
}

/**
 * @brief Submits a task without input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @since 10
 * @version 1.0
 */
static inline void submit(std::function<void()>&& func)
{
    return ffrt_submit_base(create_function_wrapper(std::move(func)), nullptr, nullptr, nullptr);
}

/**
 * @brief Submits a task with input dependencies only.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(std::function<void()>&& func, std::initializer_list<dependence> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(std::function<void()>&& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @since 10
 * @version 1.0
 */
static inline void submit(std::function<void()>&& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

/**
 * @brief Submits a task with input dependencies only.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(std::function<void()>&& func, const std::vector<dependence>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(std::function<void()>&& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @since 10
 * @version 1.0
 */
static inline void submit(std::function<void()>&& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

/**
 * @brief Submits a task without input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @since 10
 * @version 1.0
 */
static inline void submit(const std::function<void()>& func)
{
    return ffrt_submit_base(create_function_wrapper(func), nullptr, nullptr, nullptr);
}

/**
 * @brief Submits a task with input dependencies only.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(const std::function<void()>& func, std::initializer_list<dependence> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(const std::function<void()>& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @since 10
 * @version 1.0
 */
static inline void submit(const std::function<void()>& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, &attr);
}

/**
 * @brief Submits a task with input dependencies only.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(const std::function<void()>& func, const std::vector<dependence>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @since 10
 * @version 1.0
 */
static inline void submit(const std::function<void()>& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @since 10
 * @version 1.0
 */
static inline void submit(const std::function<void()>& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, &attr);
}

/**
 * @brief Submits a task without input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(std::function<void()>&& func)
{
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), nullptr, nullptr, nullptr);
}

/**
 * @brief Submits a task with input dependencies only, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(std::function<void()>&& func, std::initializer_list<dependence> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(std::function<void()>&& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(std::function<void()>&& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

/**
 * @brief Submits a task with input dependencies only, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(std::function<void()>&& func, const std::vector<dependence>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(std::function<void()>&& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(std::function<void()>&& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

/**
 * @brief Submits a task without input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(const std::function<void()>& func)
{
    return ffrt_submit_h_base(create_function_wrapper(func), nullptr, nullptr, nullptr);
}

/**
 * @brief Submits a task with input dependencies only, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(const std::function<void()>& func, std::initializer_list<dependence> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(const std::function<void()>& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(const std::function<void()>& func, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps,  const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, &attr);
}

/**
 * @brief Submits a task with input dependencies only, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(const std::function<void()>& func, const std::vector<dependence>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

/**
 * @brief Submits a task with input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(const std::function<void()>& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, nullptr);
}

/**
 * @brief Submits a task with a specified attribute and input and output dependencies, and obtains a task handle.
 *
 * @param func Indicates a task executor function closure.
 * @param in_deps Indicates a pointer to the input dependencies.
 * @param out_deps Indicates a pointer to the output dependencies.
 * @param attr Indicates a task attribute.
 * @return Returns a non-null task handle if the task is submitted;
           returns a null pointer otherwise.
 * @since 10
 * @version 1.0
 */
static inline task_handle submit_h(const std::function<void()>& func, const std::vector<dependence>& in_deps,
    const std::vector<dependence>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, &attr);
}

/**
 * @brief Waits until all submitted tasks are complete.
 *
 * @since 10
 * @version 1.0
 */
static inline void wait()
{
    ffrt_wait();
}

/**
 * @brief Waits until dependent tasks are complete.
 *
 * @param deps Indicates a pointer to the dependent tasks.
 * @since 10
 * @version 1.0
 */
static inline void wait(std::initializer_list<dependence> deps)
{
    ffrt_deps_t d{static_cast<uint32_t>(deps.size()), deps.begin()};
    ffrt_wait_deps(&d);
}

/**
 * @brief Waits until dependent tasks are complete.
 *
 * @param deps Indicates a pointer to the dependent tasks.
 * @since 10
 * @version 1.0
 */
static inline void wait(const std::vector<dependence>& deps)
{
    ffrt_deps_t d{static_cast<uint32_t>(deps.size()), deps.data()};
    ffrt_wait_deps(&d);
}

/**
 * @brief Sets the thread stack size of a specified QoS level.
 *
 * @param qos_ Indicates the QoS.
 * @param stack_size Indicates the thread stack size.
 * @return Returns ffrt_success if the stack size set success;
           returns ffrt_error_inval if qos_ or stack_size invalid;
           returns ffrt_error otherwise.
 * @since 10
 * @version 1.0
 */
static inline ffrt_error_t set_worker_stack_size(qos qos_, size_t stack_size)
{
    return ffrt_set_worker_stack_size(qos_, stack_size);
}

namespace this_task {
static inline int update_qos(qos qos_)
{
    return ffrt_this_task_update_qos(qos_);
}

/**
 * @brief Obtains the ID of this task.
 *
 * @return Returns the task ID.
 * @since 10
 * @version 1.0
 */
static inline uint64_t get_id()
{
    return ffrt_this_task_get_id();
}
} // namespace this_task
} // namespace ffrt
#endif
