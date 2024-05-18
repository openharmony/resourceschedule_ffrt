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
#include <climits>

#include "ffrt_inner.h"
#include "internal_inc/osal.h"
#include "sync/io_poller.h"
#include "qos.h"
#include "sched/task_scheduler.h"
#include "task_attr_private.h"
#include "internal_inc/config.h"
#include "eu/osattr_manager.h"
#include "eu/worker_thread.h"
#include "dfx/log/ffrt_log_api.h"
#include "dfx/watchdog/watchdog_util.h"
#include "queue/serial_task.h"
#include "eu/func_manager.h"
#include "util/ffrt_facade.h"
#include "eu/sexecute_unit.h"

#ifdef FFRT_IO_TASK_SCHEDULER
#include "core/task_io.h"
#include "sync/poller.h"
#include "util/spmc_queue.h"
#endif
#include "tm/task_factory.h"

namespace ffrt {
inline void submit_impl(bool has_handle, ffrt_task_handle_t &handle, ffrt_function_header_t *f,
    const ffrt_deps_t *ins, const ffrt_deps_t *outs, const task_attr_private *attr)
{
    FFRTFacade::GetDMInstance().onSubmit(has_handle, handle, f, ins, outs, attr);
}

API_ATTRIBUTE((visibility("default")))
void sync_io(int fd)
{
    ffrt_wait_fd(fd);
}

API_ATTRIBUTE((visibility("default")))
void set_trace_tag(const char* name)
{
    CPUEUTask* curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask != nullptr) {
        curTask->SetTraceTag(name);
    }
}

API_ATTRIBUTE((visibility("default")))
void clear_trace_tag()
{
    CPUEUTask* curTask = ffrt::ExecuteCtx::Cur()->task;
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
        FFRT_LOGD("submit task delay time [%d us] has ended.", delayUs);
    };
    ffrt_function_header_t *delay_func = create_function_wrapper(std::move(func));
    submit_impl(true, handle, delay_func, nullptr, nullptr, reinterpret_cast<task_attr_private *>(p));
}
} // namespace ffrt

#ifdef __cplusplus
extern "C" {
#endif
API_ATTRIBUTE((visibility("default")))
int ffrt_task_attr_init(ffrt_task_attr_t *attr)
{
    if (unlikely(!attr)) {
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
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    auto p = reinterpret_cast<ffrt::task_attr_private *>(attr);
    p->~task_attr_private();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_name(ffrt_task_attr_t *attr, const char *name)
{
    if (unlikely(!attr || !name)) {
        FFRT_LOGE("attr or name not valid");
        return;
    }
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->name_ = name;
}

API_ATTRIBUTE((visibility("default")))
const char *ffrt_task_attr_get_name(const ffrt_task_attr_t *attr)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return nullptr;
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return (reinterpret_cast<ffrt::task_attr_private *>(p))->name_.c_str();
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_qos(ffrt_task_attr_t *attr, ffrt_qos_t qos)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    if (ffrt::GetFuncQosMap() == nullptr) {
        FFRT_LOGE("FuncQosMap has not regist");
        return;
    }
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->qos_ = ffrt::GetFuncQosMap()(qos);
}

API_ATTRIBUTE((visibility("default")))
ffrt_qos_t ffrt_task_attr_get_qos(const ffrt_task_attr_t *attr)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return static_cast<int>(ffrt_qos_default);
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return (reinterpret_cast<ffrt::task_attr_private *>(p))->qos_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_delay(ffrt_task_attr_t *attr, uint64_t delay_us)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->delay_ = delay_us;
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_task_attr_get_delay(const ffrt_task_attr_t *attr)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return 0;
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return (reinterpret_cast<ffrt::task_attr_private *>(p))->delay_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_timeout(ffrt_task_attr_t *attr, uint64_t timeout_ms)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->timeout_ = timeout_ms;
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_task_attr_get_timeout(const ffrt_task_attr_t *attr)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return 0;
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return (reinterpret_cast<ffrt::task_attr_private *>(p))->timeout_;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_queue_priority(ffrt_task_attr_t* attr, ffrt_queue_priority_t priority)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }

    if (priority < ffrt_queue_priority_immediate || priority > ffrt_queue_priority_idle) {
        FFRT_LOGE("priority should be a valid priority");
        return;
    }

    (reinterpret_cast<ffrt::task_attr_private *>(attr))->prio_ = priority;
}

API_ATTRIBUTE((visibility("default")))
ffrt_queue_priority_t ffrt_task_attr_get_queue_priority(const ffrt_task_attr_t* attr)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return ffrt_queue_priority_immediate;
    }
    ffrt_task_attr_t *p = const_cast<ffrt_task_attr_t *>(attr);
    return static_cast<ffrt_queue_priority_t>((reinterpret_cast<ffrt::task_attr_private *>(p))->prio_);
}

// submit
API_ATTRIBUTE((visibility("default")))
void *ffrt_alloc_auto_managed_function_storage_base(ffrt_function_kind_t kind)
{
    if (kind == ffrt_function_kind_general) {
        return ffrt::TaskFactory::Alloc()->func_storage;
    }
    return ffrt::SimpleAllocator<ffrt::SerialTask>::allocMem()->func_storage;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_submit_base(ffrt_function_header_t *f, const ffrt_deps_t *in_deps, const ffrt_deps_t *out_deps,
    const ffrt_task_attr_t *attr)
{
    if (unlikely(!f)) {
        FFRT_LOGE("function handler should not be empty");
        return;
    }
    ffrt_task_handle_t handle;
    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    if (likely(attr == nullptr || ffrt_task_attr_get_delay(attr) == 0)) {
        ffrt::submit_impl(false, handle, f, in_deps, out_deps, p);
        return;
    }

    // task after delay
    ffrt_task_handle_t delay_handle;
    uint64_t timeout = p->timeout_;
    p->timeout_ = 0;
    ffrt::create_delay_deps(delay_handle, in_deps, out_deps, p);
    p->timeout_ = timeout;
    std::vector<ffrt_dependence_t> deps = {{ffrt_dependence_task, delay_handle}};
    ffrt_deps_t delay_deps {static_cast<uint32_t>(deps.size()), deps.data()};
    ffrt::submit_impl(false, handle, f, &delay_deps, nullptr, p);
    ffrt_task_handle_destroy(delay_handle);
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_handle_t ffrt_submit_h_base(ffrt_function_header_t *f, const ffrt_deps_t *in_deps,
    const ffrt_deps_t *out_deps, const ffrt_task_attr_t *attr)
{
    if (unlikely(!f)) {
        FFRT_LOGE("function handler should not be empty");
        return nullptr;
    }
    ffrt_task_handle_t handle = nullptr;
    ffrt::task_attr_private *p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    if (likely(attr == nullptr || ffrt_task_attr_get_delay(attr) == 0)) {
        ffrt::submit_impl(true, handle, f, in_deps, out_deps, p);
        return handle;
    }

    // task after delay
    ffrt_task_handle_t delay_handle = nullptr;
    uint64_t timeout = p->timeout_;
    p->timeout_ = 0;
    ffrt::create_delay_deps(delay_handle, in_deps, out_deps, p);
    p->timeout_ = timeout;
    std::vector<ffrt_dependence_t> deps = {{ffrt_dependence_task, delay_handle}};
    ffrt_deps_t delay_deps {static_cast<uint32_t>(deps.size()), deps.data()};
    ffrt::submit_impl(true, handle, f, &delay_deps, nullptr, p);
    ffrt_task_handle_destroy(delay_handle);
    return handle;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_handle_destroy(ffrt_task_handle_t handle)
{
    if (handle == nullptr) {
        FFRT_LOGE("input task handle is invalid");
        return;
    }
    static_cast<ffrt::CPUEUTask*>(handle)->DecDeleteRef();
}

// wait
API_ATTRIBUTE((visibility("default")))
void ffrt_wait_deps(const ffrt_deps_t *deps)
{
    if (unlikely(!deps)) {
        FFRT_LOGE("deps should not be empty");
        return;
    }
    std::vector<ffrt_dependence_t> v(deps->len);
    for (uint64_t i = 0; i < deps->len; ++i) {
        v[i] = deps->items[i];
    }
    ffrt_deps_t d = { deps->len, v.data() };
    ffrt::FFRTFacade::GetDMInstance().onWait(&d);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_wait()
{
    ffrt::FFRTFacade::GetDMInstance().onWait();
}

API_ATTRIBUTE((visibility("default")))
int ffrt_set_cgroup_attr(ffrt_qos_t qos, ffrt_os_sched_attr *attr)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should not be empty");
        return -1;
    }
    if (ffrt::GetFuncQosMap() == nullptr) {
        FFRT_LOGE("FuncQosMap has not regist");
        return -1;
    }
    ffrt::QoS _qos = ffrt::QoS(ffrt::GetFuncQosMap()(qos));
    return ffrt::OSAttrManager::Instance()->UpdateSchedAttr(_qos, attr);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_restore_qos_config()
{
    ffrt::WorkerGroupCtl *wgCtl = ffrt::FFRTFacade::GetEUInstance().GetGroupCtl();
    for (auto qos = ffrt::QoS::Min(); qos < ffrt::QoS::Max(); ++qos) {
        std::unique_lock<std::shared_mutex> lck(wgCtl[qos].tgMutex);
        for (auto& thread : wgCtl[qos].threads) {
            ffrt::SetThreadAttr(thread.first, ffrt::QoS(qos));
        }
    }
}

API_ATTRIBUTE((visibility("default")))
int ffrt_set_cpu_worker_max_num(ffrt_qos_t qos, uint32_t num)
{
    if (ffrt::GetFuncQosMap() == nullptr) {
        FFRT_LOGE("FuncQosMap has not regist");
        return -1;
    }
    ffrt::QoS _qos = ffrt::QoS(ffrt::GetFuncQosMap()(qos));
    if (((qos != ffrt::qos_default) && (_qos() == ffrt::qos_default)) || (qos <= ffrt::qos_inherit)) {
        FFRT_LOGE("qos[%d] is invalid.", qos);
        return -1;
    }
    ffrt::CPUMonitor *monitor = ffrt::FFRTFacade::GetEUInstance().GetCPUMonitor();
    return monitor->SetWorkerMaxNum(_qos, num);
}

API_ATTRIBUTE((visibility("default")))
ffrt_error_t ffrt_set_worker_stack_size(ffrt_qos_t qos, size_t stack_size)
{
    if (qos < ffrt::QoS::Min() || qos >= ffrt::QoS::Max() || stack_size < PTHREAD_STACK_MIN) {
        FFRT_LOGE("qos [%d] or stack size [%d] is invalid.", qos, stack_size);
        return ffrt_error_inval;
    }

    ffrt::WorkerGroupCtl* groupCtl = ffrt::FFRTFacade::GetEUInstance().GetGroupCtl();
    if (!groupCtl[qos].threads.empty()) {
        FFRT_LOGE("Stack size can be set only when there is no worker.");
        return ffrt_error;
    }

    int pageSize = getpagesize();
    if (pageSize < 0) {
        FFRT_LOGE("Invalid pagesize : %d", pageSize);
        return ffrt_error;
    }

    groupCtl[qos].workerStackSize = (stack_size - 1 + static_cast<size_t>(pageSize)) &
        -(static_cast<size_t>(pageSize));

    return ffrt_success;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_this_task_update_qos(ffrt_qos_t qos)
{
    if (ffrt::GetFuncQosMap() == nullptr) {
        FFRT_LOGE("FuncQosMap has not regist");
        return 1;
    }
    ffrt::QoS _qos = ffrt::QoS(ffrt::GetFuncQosMap()(qos));
    auto curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask == nullptr) {
        FFRT_LOGW("task is nullptr");
        return 1;
    }
    FFRT_COND_DO_ERR((curTask->type != ffrt_normal_task), return 1, "update qos task type invalid");
    if (_qos() == curTask->qos) {
        FFRT_LOGW("the target qos is equal to current qos, no need update");
        return 0;
    }

    curTask->SetQos(_qos);
    ffrt_yield();

    return 0;
}

API_ATTRIBUTE((visibility("default")))
ffrt_qos_t ffrt_this_task_get_qos()
{
    if (ffrt::ExecuteCtx::Cur()->task == nullptr) {
        FFRT_LOGW("task is nullptr");
        return static_cast<int>(ffrt_qos_default);
    }
    return ffrt::ExecuteCtx::Cur()->qos();
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_this_task_get_id()
{
    auto curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask == nullptr) {
        return 0;
    }

    FFRT_COND_DO_ERR((curTask->type != ffrt_normal_task), return 0, "get id task type invalid");
    return curTask->gid;
}

API_ATTRIBUTE((visibility("default")))
int64_t ffrt_this_queue_get_id()
{
    auto curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask == nullptr || curTask->type != ffrt_serial_task) {
        // not serial queue task
        return -1;
    }

    ffrt::SerialTask* task = reinterpret_cast<ffrt::SerialTask*>(curTask);
    return task->GetQueueId();
}

API_ATTRIBUTE((visibility("default")))
int ffrt_skip(ffrt_task_handle_t handle)
{
    if (!handle) {
        FFRT_LOGE("input ffrt task handle is invalid.");
        return -1;
    }
    ffrt::CPUEUTask *task = static_cast<ffrt::CPUEUTask*>(handle);
    auto exp = ffrt::SkipStatus::SUBMITTED;
    if (__atomic_compare_exchange_n(&task->skipped, &exp, ffrt::SkipStatus::SKIPPED, 0, __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED)) {
        return 0;
    }
    FFRT_LOGE("skip task [%lu] faild", task->gid);
    return 1;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_executor_task_submit(ffrt_executor_task_t* task, const ffrt_task_attr_t* attr)
{
    if (task == nullptr) {
        FFRT_LOGE("function handler should not be empty");
        return;
    }
    ffrt::task_attr_private* p = reinterpret_cast<ffrt::task_attr_private *>(const_cast<ffrt_task_attr_t *>(attr));
    if (likely(attr == nullptr || ffrt_task_attr_get_delay(attr) == 0)) {
        ffrt::FFRTFacade::GetDMInstance().onSubmitUV(task, p);
        return;
    }
    FFRT_LOGE("uv function not supports delay");
}

API_ATTRIBUTE((visibility("default")))
void ffrt_executor_task_register_func(ffrt_executor_task_func func, ffrt_executor_task_type_t type)
{
    ffrt::FuncManager* func_mg = ffrt::FuncManager::Instance();
    func_mg->insert(type, func);
}

API_ATTRIBUTE((visibility("default")))
int ffrt_executor_task_cancel(ffrt_executor_task_t* task, const ffrt_qos_t qos)
{
    if (task == nullptr) {
        FFRT_LOGE("function handler should not be empty");
        return 0;
    }
    ffrt::QoS _qos = ffrt::QoS(qos);

    ffrt::LinkedList* node = reinterpret_cast<ffrt::LinkedList *>(&task->wq);
    ffrt::FFRTFacade::GetDMInstance();
    ffrt::FFRTScheduler* sch = ffrt::FFRTScheduler::Instance();
    return static_cast<int>(sch->RemoveNode(node, _qos));
}

API_ATTRIBUTE((visibility("default")))
void* ffrt_get_cur_task()
{
    return ffrt::ExecuteCtx::Cur()->task;
}

API_ATTRIBUTE((visibility("default")))
bool ffrt_get_current_coroutine_stack(void** stackAddr, size_t* size)
{
    if (stackAddr == nullptr || size == nullptr) {
        return false;
    }

    if (!ffrt::USE_COROUTINE) {
        return false;
    }
    auto curTask = ffrt::ExecuteCtx::Cur()->task;
    if (curTask != nullptr) {
        auto co = curTask->coRoutine;
        if (co) {
            *size = co->stkMem.size;
            *stackAddr = (void*)((char*)co + sizeof(CoRoutine) - 8);
            return true;
        }
    }
    return false;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_attr_set_local(ffrt_task_attr_t* attr, bool task_local)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return;
    }
    (reinterpret_cast<ffrt::task_attr_private *>(attr))->taskLocal_ = task_local;
}

API_ATTRIBUTE((visibility("default")))
bool ffrt_task_attr_get_local(ffrt_task_attr_t* attr)
{
    if (unlikely(!attr)) {
        FFRT_LOGE("attr should be a valid address");
        return false;
    }
    return (reinterpret_cast<ffrt::task_attr_private *>(attr))->taskLocal_;
}
#ifdef __cplusplus
}
#endif