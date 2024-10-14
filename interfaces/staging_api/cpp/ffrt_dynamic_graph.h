/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef FFRT_DYNAMIC_GRAPH_CPP_API_H
#define FFRT_DYNAMIC_GRAPH_CPP_API_H
#include <vector>
#include <functional>
#include <c/ffrt_dynamic_graph.h>
#include <cpp/task.h>

namespace ffrt {
static inline bool hcs_get_capability(uint32_t hw_property_bitmap)
{
    return ffrt_hcs_get_capability(hw_property_bitmap);
}

static inline task_handle hcs_submit_h(const ffrt_hcs_task_t *task)
{
    return ffrt_hcs_submit_h(task, nullptr, nullptr, nullptr);
}

static inline task_handle hcs_submit_h(const ffrt_hcs_task_t *task, std::initializer_list<dependence> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    return ffrt_hcs_submit_h(task, &in, nullptr, nullptr);
}

static inline task_handle hcs_submit_h(const ffrt_hcs_task_t *task, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_hcs_submit_h(task, &in, &out, nullptr);
}

static inline task_handle hcs_submit_h(const ffrt_hcs_task_t *task, std::initializer_list<dependence> in_deps,
    std::initializer_list<dependence> out_deps, const task_attr &attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), in_deps.begin()};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), out_deps.begin()};
    return ffrt_hcs_submit_h(task, &in, &out, &attr);
}

static inline task_handle hcs_submit_h(const ffrt_hcs_task_t *task, std::vector<dependence> in_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), &(*in_deps.begin())};
    return ffrt_hcs_submit_h(task, &in, nullptr, nullptr);
}

static inline task_handle hcs_submit_h(const ffrt_hcs_task_t *task, std::vector<dependence> in_deps,
    std::initializer_list<dependence> out_deps)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), &(*in_deps.begin())};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), &(*out_deps.begin())};
    return ffrt_hcs_submit_h(task, &in, &out, nullptr);
}

static inline task_handle hcs_submit_h(const ffrt_hcs_task_t *task, std::vector<dependence> in_deps,
    std::initializer_list<dependence> out_deps, const task_attr &attr)
{
    ffrt_deps_t in{static_cast<uint32_t>(in_deps.size()), &(*in_deps.begin())};
    ffrt_deps_t out{static_cast<uint32_t>(out_deps.size()), &(*out_deps.begin())};
    return ffrt_hcs_submit_h(task, &in, &out, &attr);
}

static inline void hcs_wait()
{
    return ffrt_hcs_wait();
}

static inline int hcs_wait(std::initializer_list<dependence> deps)
{
    ffrt_deps_t d{static_cast<uint32_t>(deps.size(), deps.begin())};
    return ffrt_hcs_wait_deps(&d);
}

static inline void hcs_wait(std::vector<dependence> deps)
{
    ffrt_deps_t d{static_cast<uint32_t>(deps.size(), &(*deps.begin()))};
    return ffrt_hcs_wait_deps(&d);
}

} // namespace ffrt
#endif