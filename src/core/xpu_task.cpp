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

#include "c/type_def_ext.h"
#include "c/ffrt_dynamic_graph.h"
#include "c/ffrt_types.h"
#include "cpp/task.h"
#include "dfx/log/ffrt_log_api.h"
#include "dm/dependence_manager.h"
#include "internal_inc/osal.h"
#include "util/ffrt_facade.h"


using namespace ffrt;

namespace {
inline ffrt_function_header_t* create_callable_wrapper(const ffrt_callable_t& call)
{
    std::function<void()> &&func = [&]() {
        call.exec(call.args);
        if (call.destory) {
            call.destory(call.args);
        }
    };
    return create_function_wrapper(std::move(func));
}
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_hcs_submit_h(const ffrt_hcs_task_t *task, const ffrt_deps_t *in_deps,
    const ffrt_deps_t *out_deps, const ffrt_task_attr_t *attr)
{
    FFRT_COND_DO_ERR((task == nullptr), return nullptr, "hcs_task is nullptr");
    task_attr_private *p = reinterpret_cast<task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    // weak dependency by default, if later version neads, the strong/weak dependency can be selected.
    // ref https://wiki.huawei.com/domains/13811/wiki/48344/WIKI202404183354829
    if (task->pre_run.exec != nullptr) {
        (void)ffrt_submit_h_base(create_callable_wrapper(task->pre_run), in_deps, nullptr, attr);
    }

    ffrt_task_handle_t handle = nullptr;
    DependenceManager::Instance().onSubmitDev(task, true, handle, in_deps, out_deps, p);
    FFRT_COND_DO_ERR((handle == nullptr), return nullptr, "invalid XPU task");

    if (task->post_run.exec != nullptr) {
        std::vector<ffrt_dependence_t> deps = {{ffrt_dependence_task, handle}};
        ffrt_deps_t inDeps{static_cast<uint32_t>(deps.size()), deps.data()};
        (void)ffrt_submit_h_base(create_callable_wrapper(task->post_run), &inDeps, nullptr, attr);
    }

    return handle;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_hcs_wait_deps(const ffrt_deps_t *deps)
{
    if (unlikely(!deps)) {
        FFRT_LOGE("task should not be empty");
        return FFRT_HW_PARAM_WRONG;
    }
    DependenceManager::Instance().onWait(deps);
    return DependenceManager::Instance().onExecResults(deps);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_hcs_wait()
{
    DependenceManager::Instance().onWait(); // 与CPU任务等待相同
}