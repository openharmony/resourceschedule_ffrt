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
#include "core/dependence_manager.h"
#include "util/slab.h"

namespace ffrt {

static std::atomic<uint64_t> s_gid(0);
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
    : parent(parent), rank(id), identity(identity), gid(++s_gid), qos(qos)
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
    if (parent != nullptr) {
        FFRT_SUBMIT_MARKER(label, gid);
    }
    FFRT_LOGI("create task name:%s gid=%lu", label.c_str(), gid);
}

void TaskCtx::ChargeQoSSubmit(const QoS& qos)
{
    if (qos == qos_inherit) {
        if (!this->IsRoot()) {
            this->qos = parent->qos;
        }
        FFRT_LOGD("Change task %s QoS %d", label.c_str(), static_cast<int>(this->qos));
    } else {
        this->qos = qos;
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

#ifdef EU_COROUTINE
    if (parent->parent == nullptr) {
        parent->childWaitCond_.notify_all();
    } else {
        parent->UpdateState(TaskState::READY);
    }
#else
    parent->childWaitCond_.notify_all();
#endif
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

#ifdef EU_COROUTINE
    if (parent == nullptr) {
        dataWaitCond_.notify_all();
    } else {
        this->UpdateState(TaskState::READY);
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
    }
#else
    dataWaitCond_.notify_all();
#endif
}

bool TaskCtx::IsPrevTask(const TaskCtx* task) const
{
    std::list<uint64_t> ThisTaskIds;
    std::list<uint64_t> OtherTaskIds;
    const TaskCtx* now = this;
    while (now != DependenceManager::Root()) {
        ThisTaskIds.push_front(now->rank);
        now = now->parent;
    }
    while (task != DependenceManager::Root()) {
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
} /* namespace ffrt */