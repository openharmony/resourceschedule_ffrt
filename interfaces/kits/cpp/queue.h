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

    // set qos
    inline queue_attr& qos(enum qos qos)
    {
        ffrt_queue_attr_set_qos(this, static_cast<ffrt_qos_t>(qos));
        return *this;
    }

    // get qos
    inline enum qos qos() const
    {
        return static_cast<enum qos>(ffrt_queue_attr_get_qos(this));
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

    // set timeoutCb
    inline queue_attr& timeoutCb(std::function<void()>& func)
    {
        ffrt_queue_attr_set_timeoutCb(this, create_function_wrapper(func, ffrt_function_kind_queue));
        return *this;
    }

    // get timeoutCb
    inline ffrt_function_header_t* timeoutCb() const
    {
        return ffrt_queue_attr_get_timeoutCb(this);
    }
};

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

    // submit
    inline void submit(std::function<void()>& func)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), nullptr);
    }

    inline void submit(std::function<void()>& func, const task_attr& attr)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), &attr);
    }

    inline void submit(std::function<void()>&& func)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), nullptr);
    }

    inline void submit(std::function<void()>&& func, const task_attr& attr)
    {
        ffrt_queue_submit(queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), &attr);
    }

    // submit_h
    inline task_handle submit_h(std::function<void()>& func)
    {
        return ffrt_queue_submit_h(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), nullptr);
    }

    inline task_handle submit_h(std::function<void()>& func, const task_attr& attr)
    {
        return ffrt_queue_submit_h(queue_handle, create_function_wrapper(func, ffrt_function_kind_queue), &attr);
    }

    inline task_handle submit_h(std::function<void()>&& func)
    {
        return ffrt_queue_submit_h(
            queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), nullptr);
    }

    inline task_handle submit_h(std::function<void()>&& func, const task_attr& attr)
    {
        return ffrt_queue_submit_h(
            queue_handle, create_function_wrapper(std::move(func), ffrt_function_kind_queue), &attr);
    }

    // cancel
    inline int cancel(task_handle& handle)
    {
        return ffrt_queue_cancel(handle);
    }

    // wait
    inline void wait(task_handle& handle)
    {
        return ffrt_queue_wait(handle);
    }

private:
    ffrt_queue_t queue_handle = nullptr;
};
} // namespace ffrt

#endif // FFRT_API_CPP_QUEUE_H