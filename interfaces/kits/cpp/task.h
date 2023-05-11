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
#ifndef FFRT_API_CPP_TASK_H
#define FFRT_API_CPP_TASK_H
#include <vector>
#include <string>
#include <functional>
#include <memory>
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
    @brief set task name
    */
    inline task_attr& name(const char* name)
    {
        ffrt_task_attr_set_name(this, name);
        return *this;
    }

    /**
    @brief get task name
    */
    inline const char* name() const
    {
        return ffrt_task_attr_get_name(this);
    }

    /**
    @brief set qos
    */
    inline task_attr& qos(enum qos qos)
    {
        ffrt_task_attr_set_qos(this, static_cast<ffrt_qos_t>(qos));
        return *this;
    }

    /**
    @brief get qos
    */
    inline enum qos qos() const
    {
        return static_cast<enum qos>(ffrt_task_attr_get_qos(this));
    }

    /**
    @brief set task delay
    */
    inline task_attr& delay(uint64_t delay_us)
    {
        ffrt_task_attr_set_delay(this, delay_us);
        return *this;
    }

    /**
    @brief get task name
    */
    inline uint64_t delay() const
    {
        return ffrt_task_attr_get_delay(this);
    }
};

// ffrt_queue_destroy_task_handle
typedef void (*destroy_func_t)(ffrt_task_handle_t);

class task_handle {
public:
    task_handle(destroy_func_t func = ffrt_task_handle_destroy) : p(nullptr), destroy_func(func)
    {
    }
    task_handle(ffrt_task_handle_t p, destroy_func_t func = ffrt_task_handle_destroy) : p(p), destroy_func(func)
    {
    }

    ~task_handle()
    {
        if (p && destroy_func) {
            destroy_func(p);
        }
    }

    task_handle(task_handle const&) = delete;
    void operator=(task_handle const&) = delete;

    inline task_handle(task_handle&& h)
    {
        *this = std::move(h);
    }

    inline task_handle& operator=(task_handle&& h)
    {
        if (p && destroy_func) {
            destroy_func(p);
        }
        p = h.p;
        h.p = nullptr;
        return *this;
    }

    inline operator void* () const
    {
        return p;
    }

private:
    ffrt_task_handle_t p = nullptr;
    destroy_func_t destroy_func = nullptr;
};

template<class T>
struct function {
    template<class CT>
    function(ffrt_function_header_t h, CT&& c) : header(h), closure(std::forward<CT>(c)) {}
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
inline ffrt_function_header_t* create_function_wrapper(T&& func)
{
    using function_type = function<std::decay_t<T>>;
    static_assert(sizeof(function_type) <= ffrt_auto_managed_function_storage_size,
        "size of function must be less than ffrt_auto_managed_function_storage_size");

    auto p = ffrt_alloc_auto_free_function_storage_base();
    auto f =
        new (p)function_type({ exec_function_wrapper<T>, destroy_function_wrapper<T>, { 0 } }, std::forward<T>(func));
    return reinterpret_cast<ffrt_function_header_t *>(f);
}

/**
@brief submit a task with the given func and its dependency
*/
static inline void submit(std::function<void()>&& func)
{
    return ffrt_submit_base(create_function_wrapper(std::move(func)), nullptr, nullptr, nullptr);
}

static inline void submit(std::function<void()>&& func, std::initializer_list<const void*> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

static inline void submit(std::function<void()>&& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

static inline void submit(std::function<void()>&& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

static inline void submit(std::function<void()>&& func, const std::vector<const void*>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

static inline void submit(std::function<void()>&& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

static inline void submit(std::function<void()>&& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

static inline void submit(const std::function<void()>& func)
{
    return ffrt_submit_base(create_function_wrapper(func), nullptr, nullptr, nullptr);
}

static inline void submit(const std::function<void()>& func, std::initializer_list<const void*> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

static inline void submit(const std::function<void()>& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, nullptr);
}

static inline void submit(const std::function<void()>& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, &attr);
}

static inline void submit(const std::function<void()>& func, const std::vector<const void*>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

static inline void submit(const std::function<void()>& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, nullptr);
}

static inline void submit(const std::function<void()>& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_base(create_function_wrapper(func), &in, &out, &attr);
}

/**
@brief submit and return task handle
*/
static inline task_handle submit_h(std::function<void()>&& func)
{
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), nullptr, nullptr, nullptr);
}

static inline task_handle submit_h(std::function<void()>&& func, std::initializer_list<const void*> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

static inline task_handle submit_h(std::function<void()>&& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

static inline task_handle submit_h(std::function<void()>&& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

static inline task_handle submit_h(std::function<void()>&& func, const std::vector<const void*>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, nullptr, nullptr);
}

static inline task_handle submit_h(std::function<void()>&& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, nullptr);
}

static inline task_handle submit_h(std::function<void()>&& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(std::move(func)), &in, &out, &attr);
}

static inline task_handle submit_h(const std::function<void()>& func)
{
    return ffrt_submit_h_base(create_function_wrapper(func), nullptr, nullptr, nullptr);
}

static inline task_handle submit_h(const std::function<void()>& func, std::initializer_list<const void*> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

static inline task_handle submit_h(const std::function<void()>& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, nullptr);
}

static inline task_handle submit_h(const std::function<void()>& func, std::initializer_list<const void*> in_deps,
    std::initializer_list<const void*> out_deps,  const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, &attr);
}

static inline task_handle submit_h(const std::function<void()>& func, const std::vector<const void*>& in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, nullptr, nullptr);
}

static inline task_handle submit_h(const std::function<void()>& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, nullptr);
}

static inline task_handle submit_h(const std::function<void()>& func, const std::vector<const void*>& in_deps,
    const std::vector<const void*>& out_deps, const task_attr& attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.data()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.data()};
    return ffrt_submit_h_base(create_function_wrapper(func), &in, &out, &attr);
}

static inline int skip(task_handle &handle)
{
    return ffrt_skip(handle);
}

/**
@brief wait until all child tasks of current task to be done
*/
static inline void wait()
{
    ffrt_wait();
}

/**
@brief wait until specified data be produced
*/
static inline void wait(std::initializer_list<const void*> deps)
{
    ffrt_deps_t d{static_cast<uint32_t>(deps.size()), deps.begin()};
    ffrt_wait_deps_without_copy_base(&d);
}

static inline void wait(const std::vector<const void*>& deps)
{
    ffrt_deps_t d{static_cast<uint32_t>(deps.size()), deps.data()};
    ffrt_wait_deps(&d);
}

void sync_io(int fd);

void set_trace_tag(const std::string& name);

void clear_trace_tag();

namespace this_task {
int update_qos(enum qos qos);
uint64_t get_id();
} // namespace this_task
} // namespace ffrt
#endif
