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

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
#include <sstream>
#include "backtrace_local.h"
#endif
#include "dm/dependence_manager.h"
#include "util/slab.h"
#include "internal_inc/osal.h"
#include "internal_inc/types.h"
#include "tm/cpu_task.h"

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

SCPUEUTask::SCPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id,
    const QoS &qos)
    : CPUEUTask(attr, parent, id, qos)
{
}

void SCPUEUTask::DecDepRef()
{
    if (--depRefCnt == 0) {
        FFRT_LOGD("Undependency completed, enter ready queue, task[%lu], name[%s]", gid, label.c_str());
        FFRT_WAKE_TRACER(this->gid);
        this->UpdateState(TaskState::READY);
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
    }
}

void SCPUEUTask::DecChildRef()
{
    SCPUEUTask* parent = reinterpret_cast<SCPUEUTask*>(this->parent);
    FFRT_LOGD("DecChildRef parent task:%s, childWaitRefCnt=%u", parent->label.c_str(), parent->childWaitRefCnt.load());
    FFRT_TRACE_SCOPE(2, taskDecChildRef);
    std::unique_lock<decltype(parent->lock)> lck(parent->lock);
    parent->childWaitRefCnt--;
    if (parent->childWaitRefCnt != 0) {
        return;
    }
    if (FFRT_UNLIKELY(parent->IsRoot())) {
        RootTask *root = static_cast<RootTask *>(parent);
        if (root->thread_exit) {
            lck.unlock();
            delete root;
            return;
        }
    }

    if (!parent->IsRoot() && parent->status == TaskStatus::RELEASED && parent->childWaitRefCnt == 0) {
        FFRT_LOGD("free CPUEUTask:%s gid=%lu", parent->label.c_str(), parent->gid);
        lck.unlock();
        parent->DecDeleteRef();
        return;
    }
    if (parent->denpenceStatus != Denpence::CALL_DEPENCE) {
        return;
    }
    parent->denpenceStatus = Denpence::DEPENCE_INIT;

    bool blockThread = parent->coRoutine ? parent->coRoutine->blockType == BlockType::BLOCK_THREAD : false;
    if (!USE_COROUTINE || parent->parent == nullptr || blockThread) {
        if (blockThread) {
            parent->coRoutine->blockType = BlockType::BLOCK_COROUTINE;
        }
        parent->childWaitCond_.notify_all();
    } else {
        FFRT_WAKE_TRACER(parent->gid);
        parent->UpdateState(TaskState::READY);
    }
}

void SCPUEUTask::DecWaitDataRef()
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

    bool blockThread =
        (parent && parent->coRoutine) ? parent->coRoutine->blockType == BlockType::BLOCK_THREAD : false;
    if (!USE_COROUTINE || parent == nullptr || blockThread) {
        if (blockThread) {
            parent->coRoutine->blockType = BlockType::BLOCK_COROUTINE;
        }
        dataWaitCond_.notify_all();
    } else {
        FFRT_WAKE_TRACER(this->gid);
        this->UpdateState(TaskState::READY);
#ifdef FFRT_BBOX_ENABLE
        TaskEnQueuCounterInc();
#endif
    }
}

void SCPUEUTask::RecycleTask()
{
    std::unique_lock<decltype(lock)> lck(lock);
    if (childWaitRefCnt == 0) {
        FFRT_LOGD("free SCPUEUTask:%s gid=%lu", label.c_str(), gid);
        lck.unlock();
        DecDeleteRef();
        return;
    } else {
        status = TaskStatus::RELEASED;
    }
}

void SCPUEUTask::MultiDepenceAdd(Denpence depType)
{
    FFRT_LOGD("task(%s) ADD_DENPENCE(%s)", this->label.c_str(), DenpenceStr(depType));
    denpenceStatus = depType;
}
} /* namespace ffrt */