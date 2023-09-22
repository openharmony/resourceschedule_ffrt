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
#include "core/task_ctx.h"
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
#include <dlfcn.h>
#include <sstream>
#include "libunwind.h"
#include "backtrace_local.h"
#endif
#include "core/dependence_manager.h"
#include "util/slab.h"
#include "internal_inc/osal.h"
#include "internal_inc/types.h"

namespace ffrt {
static inline const char* DenpenceStr(Denpence d)
{
    static const char* m[] = {
        "DEPENCE_INIT",
        "DATA_DEPENCE",
        "CALL_DEPENCE",
        "CONDITION_DEPENCE",
    };
    return m[static_cast<uint64_t>(d)];
}

TaskCtx::TaskCtx(const task_attr_private *attr, TaskCtx *parent, const uint64_t &id, const char *identity,
    const QoS &qos)
    : parent(parent), rank(id), identity(identity), qos(qos)
{
    wue = nullptr;
    fq_we.task = this;
    if (attr && !attr->name_.empty()) {
        label = attr->name_;
    } else if (IsRoot()) {
        label = "root";
    } else if (parent->parent == nullptr) {
        label = "t" + std::to_string(rank);
    } else {
        label = parent->label + "." + std::to_string(rank);
    }
    if (!IsRoot()) {
        FFRT_SUBMIT_MARKER(label, gid);
    }
    FFRT_LOGD("create task name:%s gid=%lu", label.c_str(), gid);
}

void TaskCtx::SetQos(QoS& target_qos)
{
    if (target_qos == qos_inherit) {
        if (!this->IsRoot()) {
            this->qos = parent->qos;
        }
        FFRT_LOGD("Change task %s QoS %d", label.c_str(), this->qos());
    } else {
        this->qos = target_qos;
    }
}

/* For interval based userspace load tracking
 * Inherit parent's active intervals for a new task
 */
void TaskCtx::InitRelatedIntervals(TaskCtx* curr)
{
    auto& parentIntervals = curr->relatedIntervals;
    if (parentIntervals.empty()) {
        return;
    }

    for (auto it : std::as_const(parentIntervals)) {
        if (it->Qos() == curr->qos) {
            curr->relatedIntervals.insert(it);
        }
    }
}

void TaskCtx::DecDepRef()
{
    if (--depRefCnt == 0) {
        FFRT_LOGI("Undependency completed, enter ready queue, task[%lu], name[%s]", gid, label.c_str());
        FFRT_WAKE_TRACER(this->gid);
        this->UpdateState(TaskState::READY);
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
    }
}

void TaskCtx::DecChildRef()
{
    FFRT_LOGD("DecChildRef parent task:%s, childWaitRefCnt=%u", parent->label.c_str(), parent->childWaitRefCnt.load());
    FFRT_TRACE_SCOPE(2, taskDecChildRef);
    std::unique_lock<decltype(parent->lock)> lck(parent->lock);
    parent->childWaitRefCnt--;
    if (parent->childWaitRefCnt != 0) {
        return;
    }
    if (FFRT_UNLIKELY(parent->IsRoot())) {
        RootTaskCtx *root = static_cast<RootTaskCtx *>(parent);
        if (root->thread_exit == true) {
            lck.unlock();
            delete root;
            return;
        }
    }

    if (!parent->IsRoot() && parent->status == TaskStatus::RELEASED && parent->childWaitRefCnt == 0) {
        FFRT_LOGD("free TaskCtx:%s gid=%lu", parent->label.c_str(), parent->gid);
        lck.unlock();
        parent->DecDeleteRef();
        return;
    }
    if (parent->denpenceStatus != Denpence::CALL_DEPENCE) {
        return;
    }
    parent->denpenceStatus = Denpence::DEPENCE_INIT;

    if (!USE_COROUTINE || parent->parent == nullptr) {
        parent->childWaitCond_.notify_all();
    } else {
        FFRT_WAKE_TRACER(parent->gid);
        parent->UpdateState(TaskState::READY);
    }
}

void TaskCtx::DecWaitDataRef()
{
    FFRT_TRACE_SCOPE(2, taskDecWaitData);
    {
        std::lock_guard<decltype(lock)> lck(lock);
        dataWaitRefCnt--;
        if (dataWaitRefCnt != 0) {
            return;
        }
        if (denpenceStatus != Denpence::DATA_DEPENCE) {
            return;
        }
        denpenceStatus = Denpence::DEPENCE_INIT;
    }

    if (!USE_COROUTINE || parent == nullptr) {
        dataWaitCond_.notify_all();
    } else {
        FFRT_WAKE_TRACER(this->gid);
        this->UpdateState(TaskState::READY);
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
    }
}

bool TaskCtx::IsPrevTask(const TaskCtx* task) const
{
    std::list<uint64_t> ThisTaskIds;
    std::list<uint64_t> OtherTaskIds;
    const TaskCtx* now = this;
    while (now != nullptr && now != DependenceManager::Root()) {
        ThisTaskIds.push_front(now->rank);
        now = now->parent;
    }
    while (task != nullptr && task != DependenceManager::Root()) {
        OtherTaskIds.push_front(task->rank);
        task = task->parent;
    }
    return ThisTaskIds < OtherTaskIds;
}

void TaskCtx::RecycleTask()
{
    std::unique_lock<decltype(lock)> lck(lock);
    if (childWaitRefCnt == 0) {
        FFRT_LOGD("free TaskCtx:%s gid=%lu", label.c_str(), gid);
        lck.unlock();
        DecDeleteRef();
        return;
    } else {
        status = TaskStatus::RELEASED;
    }
}

void TaskCtx::MultiDepenceAdd(Denpence depType)
{
    FFRT_LOGD("task(%s) ADD_DENPENCE(%s)", this->label.c_str(), DenpenceStr(depType));
    denpenceStatus = depType;
}
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
void TaskCtx::DumpTask(TaskCtx* task, std::string& stackInfo, uint8_t flag)
{
    unw_context_t ctx;
    unw_cursor_t unw_cur;
    unw_proc_info_t unw_proc;

    if (ExecuteCtx::Cur()->task == task || task == nullptr) {
        if (flag == 0) {
            OHOS::HiviewDFX::PrintTrace(-1);
        } else {
            OHOS::HiviewDFX::GetBacktrace(stackInfo, false);
        }
        return;
    } else {
        memset(&ctx, 0, sizeof(ctx));
#if defined(__aarch64__)
        ctx.uc_mcontext.regs[UNW_AARCH64_X29] = task->coRoutine->ctx.regs[10];
        ctx.uc_mcontext.sp = task->coRoutine->ctx.regs[13];
        ctx.uc_mcontext.pc = task->coRoutine->ctx.regs[11];
#elif defined(__x86_64__)
        ctx.uc_mcontext.gregs[REG_RBX] = task->coRoutine->ctx.regs[0];
        ctx.uc_mcontext.gregs[REG_RBP] = task->coRoutine->ctx.regs[1];
        ctx.uc_mcontext.gregs[REG_RSP] = task->coRoutine->ctx.regs[6];
        ctx.uc_mcontext.gregs[REG_RIP] = *(reinterpret_cast<greg_t *>(ctx.uc_mcontext.gregs[REG_RSP] - 8));
#elif defined(__arm__)
        ctx.regs[13] = task->coRoutine->ctx.regs[0]; /* sp */
        ctx.regs[15] = task->coRoutine->ctx.regs[1]; /* pc */
        ctx.regs[14] = task->coRoutine->ctx.regs[1]; /* lr */
        ctx.regs[11] = task->coRoutine->ctx.regs[10]; /* fp */
#endif
    }

    int ret;
    int frame_id = 0;
    ret = unw_init_local(&unw_cur, &ctx);
    if (ret < 0) {
        return;
    }

    Dl_info info;
    unw_word_t prevPc = 0;
    unw_word_t offset;
    char symbol[512];
    std::ostringstream ss;
    do {
        ret = unw_get_proc_info(&unw_cur, &unw_proc);
        if (ret) {
            break;
        }

        if (prevPc == unw_proc.start_ip) {
            break;
        }

        prevPc = unw_proc.start_ip;

        ret = dladdr(reinterpret_cast<void *>(unw_proc.start_ip), &info);
        if (!ret) {
            break;
        }

        memset(symbol, 0, sizeof(symbol));
        if (unw_get_proc_name(&unw_cur, symbol, sizeof(symbol), &offset) == 0) {
            if (flag == 0) {
                FFRT_LOGE("FFRT | #%d pc: %lx %s(%p) %s", frame_id, unw_proc.start_ip, info.dli_fname,
                          (unw_proc.start_ip - reinterpret_cast<unw_word_t>(info.dli_fbase)), symbol);
            } else {
                ss << "FFRT | #" << frame_id << " pc: " << unw_proc.start_ip << " " << info.dli_fname;
                ss << "(" << (unw_proc.start_ip - reinterpret_cast<unw_word_t>(info.dli_fbase)) << ") ";
                ss << std::string(symbol, strlen(symbol)) << std::endl;
            }
        } else {
            if (flag == 0) {
                FFRT_LOGE("FFRT | #%d pc: %lx %s(%p)", frame_id, unw_proc.start_ip, info.dli_fname,
                          (unw_proc.start_ip - reinterpret_cast<unw_word_t>(info.dli_fbase)));
            } else {
                ss << "FFRT | #" << frame_id << " pc: " << unw_proc.start_ip << " " << info.dli_fname;
                ss << "(" << (unw_proc.start_ip - reinterpret_cast<unw_word_t>(info.dli_fbase)) << ")";
                ss << std::endl;
            }
        }
        ++frame_id;
    } while (unw_step(&unw_cur) > 0);

    if (flag != 0) {
        stackInfo = ss.str();
    }
    return;
}
#endif
} /* namespace ffrt */