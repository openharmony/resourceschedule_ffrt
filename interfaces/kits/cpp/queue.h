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
#ifndef FFRT_API_CPP_QUEUE_H
#define FFRT_API_CPP_QUEUE_H

#include "c/queue.h"
#include "cpp/task.h"

namespace ffrt {
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
    @brief set qos
    */
    inline queue_attr& qos(enum qos qos)
    {
        ffrt_queue_attr_set_qos(this, static_cast<ffrt_qos_t>(qos));
        return *this;
    }

    /**
    @brief get qos
    */
    inline enum qos qos() const
    {
        return static_cast<enum qos>(ffrt_queue_attr_get_qos(this));
    }
};

template<class T>
inline ffrt_function_header_t* create_queue_func_wrapper(T&& func)
{
    using function_type = function<std::decay_t<T>>;
    static_assert(sizeof(function_type) <= ffrt_auto_managed_function_storage_size,
        "size of function must be less than ffrt_auto_managed_function_storage_size");

    auto p = ffrt_alloc_auto_free_queue_func_storage_base();
    auto f =
        new (p)function_type({ exec_function_wrapper<T>, destroy_function_wrapper<T>, { 0 } }, std::forward<T>(func));
    return reinterpret_cast<ffrt_function_header_t *>(f);
}

class queue {
public:
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

    inline void submit(std::function<void()>& func, const task_attr& attr = {})
    {
        (void) ffrt_queue_submit_raw(queue_handle, create_queue_func_wrapper(func), &attr);
    }

    inline void submit(std::function<void()>&& func, const task_attr& attr = {})
    {
        (void) ffrt_queue_submit_raw(queue_handle, create_queue_func_wrapper(std::move(func)), &attr);
    }

    inline task_handle submit_h(std::function<void()>& func, const task_attr& attr = {})
    {
        ffrt_task_handle_t handle = ffrt_queue_submit_raw(queue_handle, create_queue_func_wrapper(func), &attr);
        return task_handle(handle, ffrt_queue_destroy_task_handle);
    }

    inline task_handle submit_h(std::function<void()>&& func, const task_attr& attr = {})
    {
        ffrt_task_handle_t handle =
            ffrt_queue_submit_raw(queue_handle, create_queue_func_wrapper(std::move(func)), &attr);
        return task_handle(handle, ffrt_queue_destroy_task_handle);
    }

    inline int cancel(task_handle& handle)
    {
        return ffrt_queue_cancel(handle);
    }

    inline void wait(task_handle& handle)
    {
        return ffrt_queue_wait(handle);
    }

private:
    ffrt_queue_t queue_handle = nullptr;
};
} // namespace ffrt

#endif // FFRT_API_CPP_QUEUE_H