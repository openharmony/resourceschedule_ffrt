/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <memory>
#include <vector>
#include <cstdarg>

#include "ffrt.h"
#include "cpp/task.h"

#include "internal_inc/osal.h"
#include "sync/io_poller.h"
#include "sched/qos.h"
#include "dependence_manager.h"
#include "task_attr_private.h"
#include "internal_inc/config.h"
#include "eu/osattr_manager.h"
#include "dfx/log/ffrt_log_api.h"
#include "queue/serial_task.h"

namespace ffrt {
template <int WITH_HANDLE>
inline void submit_impl(ffrt_task_handle_t &handle, ffrt_function_header_t *f,
    const ffrt_deps_t *ins, const ffrt_deps_t *outs, const task_attr_private *attr)
{
    DependenceManager::Instance()->onSubmit<WITH_HANDLE>(handle, f, ins, outs, attr);
}

API_ATTRIBUTE((visibility("default")))
void sync_io(int fd)
{
    ffrt_wait_fd(fd);
}

API_ATTRIBUTE((visibility("default")))
void set_trace_tag(const std::string& name)
{
    TaskCtx* curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask != nullptr) {
        curTask->SetTraceTag(name);
    }
}

API_ATTRIBUTE((visibility("default")))
void clear_trace_tag()
{
    TaskCtx* curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask != nullptr) {
        curTask->ClearTraceTag();
    }
}

void create_delay_deps(
    ffrt_task_handle_t &handle, const ffrt_deps_t *in_deps, const ffrt_deps_t *out_deps, task_attr_private *p)
{
    // setting dependences is not supportted for delayed task
    if (unlikely(((in_deps != nullptr) && (in_deps->len != 0)) || ((out_deps != nullptr) && (out_deps->len != 0)))) {
        FFRT_LOGE("delayed task not support dependence, in_deps/out_deps ignored.");
    }

    // delay task
    uint64_t delayUs = p->delay_;
    std::function<void()> &&func = [delayUs]() {
        this_task::sleep_for(std::chrono::microseconds(delayUs));
        FFRT_LOGI("submit task delay time [%d us] has ended.", delayUs);
    };
    ffrt_function_header_t *delay_func = create_function_wrapper(std::move(func));
    submit_impl<1>(handle, delay_func, nullptr, nullptr, reinterpret_cast<task_attr_private *>(p));
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_task_attr_init(ffrt_task_attr_t *attr)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return -1;
    }
    static_assert(sizeof(ffrt::task_attr_private) <= ffrt_task_attr_storage_size,
        "size must be less than ffrt_task_attr_storage_size");

    new (attr)ffrt::task_attr_private();
    return 0;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_destroy(ffrt_task_attr_t *attr)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    auto p = reinterpret_cast<ffrt::task_attr_private *>(attr);
    p->~task_attr_private();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_name(ffrt_task_attr_t *attr, const char *name)
{
    if (!attr || !name) {
        FFRT_LOGE("attr or name not valid");
        return;
    }
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->name_ = name;
}

API_ATTRIBUTE((visibility("default")))
const char *ffrt_task_attr_get_name(const ffrt_task_attr_t *attr)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return nullptr;
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return (reinterpret_cast<ffrt::task_attr_private *>(p))->name_.c_str();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_qos(ffrt_task_attr_t *attr, ffrt_qos_t qos)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    ffrt::QoS _qos = ffrt::QoS(qos);
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->qos_ = static_cast<ffrt::qos>(_qos());
}

API_ATTRIBUTE((visibility("default")))
ffrt_qos_t ffrt_task_attr_get_qos(const ffrt_task_attr_t *attr)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return ffrt_qos_default;
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return static_cast<ffrt_qos_t>((reinterpret_cast<ffrt::task_attr_private *>(p))->qos_);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_delay(ffrt_task_attr_t *attr, uint64_t delay_us)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->delay_ = delay_us;
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_task_attr_get_delay(const ffrt_task_attr_t *attr)
{
    if (!attr) {
        FFRT_LOGE("attr should be a valid address");
        return 0;
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return (reinterpret_cast<ffrt::task_attr_private *>(p))->delay_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_coroutine_type(ffrt_task_attr_t* attr,ffrt_coroutine_t coroutine_type)
{
    if(!attr){
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    ((ffrt::task_attr_private*)attr)->coroutine_type_=coroutine_type;
}

API_ATTRIBUTE((visibility("default")))
ffrt_coroutine_t ffrt_task_attr_get_coroutine_type(const ffrt_task_attr_t* attr)
{
    if(!attr){
        FFRT_LOGE("attr should be a valid address");
        return ffrt_coroutine_stackfull;
    }
    return ((ffrt::task_attr_private*)attr)->coroutine_type_;
}

// submit
API_ATTRIBUTE((visibility("default")))
void *ffrt_alloc_auto_managed_function_storage_base(ffrt_function_kind_t kind)
{
    if (kind == ffrt_function_kind_general) {
        return ffrt::SimpleAllocator<ffrt::TaskCtx>::allocMem()->func_storage;
    }

    return ffrt::SimpleAllocator<ffrt::SerialTask>::allocMem()->func_storage;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_submit_base(ffrt_function_header_t *f, const ffrt_deps_t *in_deps, const ffrt_deps_t *out_deps,
    const ffrt_task_attr_t *attr)
{
    if (!f) {
        FFRT_LOGE("function handler should not be empty");
        return;
    }
    ffrt_task_handle_t handle;
    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    if (likely(attr == nullptr || ffrt_task_attr_get_delay(attr) == 0)) {
        ffrt::submit_impl<0>(handle, f, in_deps, out_deps, p);
        return;
    }

    // task after delay
    ffrt_task_handle_t delay_handle;
    ffrt::create_delay_deps(delay_handle, in_deps, out_deps, p);
    std::vector<ffrt_dependence_t> deps = {{ffrt_dependence_task, delay_handle}};
    ffrt_deps_t delay_deps{static_cast<uint32_t>(deps.size()), deps.data()};
    ffrt::submit_impl<0>(handle, f, &delay_deps, nullptr, p);
    ffrt_task_handle_destroy(delay_handle);
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_submit_h_base(ffrt_function_header_t *f, const ffrt_deps_t *in_deps,
    const ffrt_deps_t *out_deps, const ffrt_task_attr_t *attr)
{
    if (!f) {
        FFRT_LOGE("function handler should not be empty");
        return nullptr;
    }
    ffrt_task_handle_t handle;
    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    if (likely(attr == nullptr || ffrt_task_attr_get_delay(attr) == 0)) {
        ffrt::submit_impl<1>(handle, f, in_deps, out_deps, p);
        return handle;
    }

    // task after delay
    ffrt_task_handle_t delay_handle;
    ffrt::create_delay_deps(delay_handle, in_deps, out_deps, p);
    std::vector<ffrt_dependence_t> deps = {{ffrt_dependence_task, delay_handle}};
    ffrt_deps_t delay_deps{static_cast<uint32_t>(deps.size()), deps.data()};
    ffrt::submit_impl<1>(handle, f, &delay_deps, nullptr, p);
    ffrt_task_handle_destroy(delay_handle);
    return handle;
}

typedef struct{
    ffrt_function_header_t header;
    ffrt_coroutine_ptr_t func;
    ffrt_function_ptr_t destroy;
    void* arg;
}ffrt_function_coroutine_t;

static ffrt_coroutine_ret_t ffrt_exec_function_coroutine_wrapper(void* t)
{
    ffrt_function_coroutine_t* f=(ffrt_function_coroutine_t*)t;
    return f->func(f->arg);
}

static void ffrt_destory_function_coroutine_wrapper(void* t)
{
    ffrt_function_coroutine_t* f=(ffrt_function_coroutine_t*)t;
    f->destroy(f->arg);  
}

static inline ffrt_function_header_t* ffrt_create_function_coroutine_wrapper(void* co,ffrt_coroutine_ptr_t exec,ffrt_function_ptr_t destroy)
{
    static_assert(sizeof(ffrt_function_coroutine_t)<=ffrt_auto_managed_function_storage_size,"size_of_ffrt_function_coroutine_t_must_be_less_than_ffrt_auto_managed_function_storage_size");
    FFRT_COND_DO_ERR((co==nullptr),return nullptr,"input valid,co==nullptr");
    FFRT_COND_DO_ERR((exec==nullptr),return nullptr,"input valid,exec==nullptr");
    FFRT_COND_DO_ERR((destroy==nullptr),return nullptr,"input valid,destroy==nullptr");
    ffrt_function_coroutine_t* f=(ffrt_function_coroutine_t*)ffrt_alloc_auto_managed_function_storage_base(ffrt_function_kind_general);
    f->header.exec=(ffrt_function_ptr_t)ffrt_exec_function_coroutine_wrapper;
    f->header.destroy=ffrt_destory_function_coroutine_wrapper;
    f->func=exec;
    f->destroy=destroy;
    f->arg=co;
    return (ffrt_function_header_t*)f;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_submit_coroutine(void* co,ffrt_coroutine_ptr_t exec,ffrt_function_ptr_t destroy,const ffrt_deps_t* in_deps,const ffrt_deps_t* out_deps,const ffrt_task_attr_t* attr)
{
    FFRT_COND_DO_ERR((((ffrt::task_attr_private*)attr)->coroutine_type_!=ffrt_coroutine_stackless),return,"input invalid,coroutine_type!=coroutine_stackless");
    ffrt_submit_base(ffrt_create_function_coroutine_wrapper(co,exec,destroy),in_deps,out_deps,attr);
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_submit_h_coroutine(void* co,ffrt_coroutine_ptr_t exec,ffrt_function_ptr_t destroy,const ffrt_deps_t* in_deps,const ffrt_deps_t* out_deps,const ffrt_task_attr_t* attr)
{
    FFRT_COND_DO_ERR((((ffrt::task_attr_private*)attr)->coroutine_type_!=ffrt_coroutine_stackless),return nullptr,"input invalid,coroutine_type!=coroutine_stackless");
    return ffrt_submit_h_base(ffrt_create_function_coroutine_wrapper(co,exec,destroy),in_deps,out_deps,attr);
}


API_ATTRIBUTE((visibility("default")))
void ffrt_task_handle_destroy(ffrt_task_handle_t handle)
{
    if (!handle) {
        FFRT_LOGE("input task handle is invalid");
        return;
    }
    static_cast<ffrt::TaskCtx*>(handle)->DecDeleteRef();
}

// wait
API_ATTRIBUTE((visibility("default")))
void ffrt_wait_deps(const ffrt_deps_t *deps)
{
    if (!deps) {
        FFRT_LOGE("deps should not be empty");
        return;
    }
    std::vector<ffrt_dependence_t> v(deps->len);
    for (uint64_t i = 0; i < deps->len; ++i) {
        v[i] = deps->items[i];
    }
    ffrt_deps_t d = { deps->len, v.data() };
    ffrt::DependenceManager::Instance()->onWait(&d);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_wait()
{
    ffrt::DependenceManager::Instance()->onWait();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_set_cpuworker_num(ffrt_qos_t qos, int num)
{
    ffrt::GlobalConfig::Instance().setCpuWorkerNum(static_cast<ffrt::qos>(qos), num);
}

API_ATTRIBUTE((visibility("default")))
int ffrt_set_cgroup_attr(ffrt_qos_t qos, ffrt_os_sched_attr *attr)
{
    if (!attr) {
        FFRT_LOGE("attr should not be empty");
        return -1;
    }
    return ffrt::OSAttrManager::Instance()->UpdateSchedAttr(static_cast<ffrt::qos>(qos), attr);
}

API_ATTRIBUTE((visibility("default")))
int ffrt_this_task_update_qos(ffrt_qos_t qos_)
{
    auto qos = static_cast<ffrt::qos>(qos_);
    auto curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask == nullptr) {
        FFRT_LOGW("task is nullptr");
        return 1;
    }

    if (qos == curTask->qos) {
        FFRT_LOGW("the target qos is euqal to current qos, no need update");
        return 0;
    }

    curTask->ChargeQoSSubmit(qos);
    ffrt_yield();

    return 0;
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_this_task_get_id()
{
    auto curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask == nullptr) {
        return 0;
    }

    return curTask->gid;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_skip(ffrt_task_handle_t handle)
{
    if (!handle) {
        FFRT_LOGE("input ffrt task handle is invalid.");
        return -1;
    }
    ffrt::TaskCtx *task = static_cast<ffrt::TaskCtx*>(handle);
    auto exp = ffrt::SkipStatus::SUBMITTED;
    if (__atomic_compare_exchange_n(&task->skipped, &exp, ffrt::SkipStatus::SKIPPED, 0, __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED)) {
        return 0;
    }
    FFRT_LOGE("skip task [%lu] faild", task->gid);
    return 1;
}

//waker
API_ATTRIBUTE((visibility("default")))
void ffrt_wake_by_handle(void* callable,ffrt_function_ptr_t exec,ffrt_function_ptr_t destroy,ffrt_task_handle_t handle)
{
    FFRT_COND_DO_ERR((co==nullptr),return nullptr,"input valid,co==nullptr");
    FFRT_COND_DO_ERR((exec==nullptr),return nullptr,"input valid,exec==nullptr");
    FFRT_COND_DO_ERR((destroy==nullptr),return nullptr,"input valid,destroy==nullptr");  
    FFRT_COND_DO_ERR((handle==nullptr),return nullptr,"input valid,handle==nullptr");  
    ffrt::TaskCtx * task=static_cast<ffrt::TaskCtx*>(CVT_HANDLE_TO_TASK(handle));

    task->lock.lock();
    FFRT_LOGW("tid:%ld ffrt_wake_by_handle and CurState=%d",syscall(SYS_gettid),task->State.CurState());
    if(task->state.CurState()!=ffrt::TaskState::State::EXITED){
        task->wake_callable_on_finish.callable=callable;
        task->wake_callable_on_finish.exec=exec;
        task->wake_callable_on_finish.destroy=destroy;
    }
    task.lock.unlock();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_set_wake_flag(bool flag)
{
    ffrt::TaskCtx * task=(ffrt::TaskCtx *)ffrt_task_get();
    task->SetWakeFlag(flag);
}

API_ATTRIBUTE((visibility("default")))
void * ffrt_task_get()
{
    return (void*)ffrt::ExecuteCtx::Cur()->task;
}

#ifdef __cplusplus
}
#endif