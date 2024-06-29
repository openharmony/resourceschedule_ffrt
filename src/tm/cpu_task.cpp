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
#include <dlfcn.h>
#include <sstream>
#include "unwinder.h"
#include "backtrace_local.h"
#endif
#include <securec.h>
#include "dm/dependence_manager.h"
#include "util/slab.h"
#include "internal_inc/osal.h"
#include "tm/task_factory.h"
#include "tm/cpu_task.h"

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
using namespace OHOS::HiviewDFX;
#endif
namespace ffrt {
void CPUEUTask::SetQos(QoS& newQos)
{
    if (newQos == qos_inherit) {
        if (!this->IsRoot()) {
            this->qos = parent->qos;
        }
        FFRT_LOGD("Change task %s QoS %d", label.c_str(), this->qos());
    } else {
        this->qos = newQos;
    }
}

void CPUEUTask::FreeMem()
{
    BboxCheckAndFreeze();
    PollerProxy::Instance()->GetPoller(qos).ClearCachedEvents(this);
#ifdef FFRT_TASK_LOCAL_ENABLE
    TaskTsdDeconstruct(this);
#endif
    ffrt::TaskFactory::Free(this);
}

void CPUEUTask::Execute()
{
    FFRT_LOGD("Execute task[%lu], name[%s]", gid, label.c_str());
    UpdateState(TaskState::RUNNING);
    auto f = reinterpret_cast<ffrt_function_header_t*>(func_storage);
    auto exp = ffrt::SkipStatus::SUBMITTED;
    if (likely(__atomic_compare_exchange_n(&skipped, &exp, ffrt::SkipStatus::EXECUTED, 0,
        __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))) {
        f->exec(f);
    }
    f->destroy(f);
    FFRT_TASKDONE_MARKER(gid);
    this->coRoutine->isTaskDone = true;
}

CPUEUTask::CPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id,
    const QoS &qos)
    : parent(parent), rank(id), qos(qos)
{
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

    taskLocal = false;
    tsd = nullptr;
    if (attr && attr->taskLocal_) {
        tsd = (void **)malloc(TSD_SIZE * sizeof(void *));
        memset_s(tsd, TSD_SIZE * sizeof(void *), 0, TSD_SIZE * sizeof(void *));
        taskLocal = attr->taskLocal_;
    }
    if (attr) {
        stack_size = std::max(attr->stackSize_, MIN_STACK_SIZE);
    }
    FFRT_LOGD("create task name:%s gid=%lu taskLocal:%d", label.c_str(), gid, taskLocal);
}

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
void CPUEUTask::DumpTask(CPUEUTask* task, std::string& stackInfo, uint8_t flag)
{
    ucontext_t ctx;

    if (ExecuteCtx::Cur()->task == task || task == nullptr) {
        if (flag == 0) {
            OHOS::HiviewDFX::PrintTrace(-1);
        } else {
            OHOS::HiviewDFX::GetBacktrace(stackInfo, false);
        }
        return;
    } else {
        memset_s(&ctx, sizeof(ctx), 0, sizeof(ctx));
#if defined(__aarch64__)
        ctx.uc_mcontext.regs[REG_AARCH64_X29] = task->coRoutine->ctx.regs[10];
        ctx.uc_mcontext.sp = task->coRoutine->ctx.regs[13];
        ctx.uc_mcontext.pc = task->coRoutine->ctx.regs[11];
#elif defined(__x86_64__)
        ctx.uc_mcontext.gregs[REG_RBX] = task->coRoutine->ctx.regs[0];
        ctx.uc_mcontext.gregs[REG_RBP] = task->coRoutine->ctx.regs[1];
        ctx.uc_mcontext.gregs[REG_RSP] = task->coRoutine->ctx.regs[6];
        ctx.uc_mcontext.gregs[REG_RIP] = *(reinterpret_cast<greg_t *>(ctx.uc_mcontext.gregs[REG_RSP] - 8));
#elif defined(__arm__)
        ctx.uc_mcontext.arm_sp = task->coRoutine->ctx.regs[0]; /* sp */
        ctx.uc_mcontext.arm_pc = task->coRoutine->ctx.regs[1]; /* pc */
        ctx.uc_mcontext.arm_lr = task->coRoutine->ctx.regs[1]; /* lr */
        ctx.uc_mcontext.arm_fp = task->coRoutine->ctx.regs[10]; /* fp */
#endif
    }

    auto co = task->coRoutine;
    uintptr_t stackBottom = (uintptr_t)((char*)co + sizeof(CoRoutine) - 8);
    uintptr_t stackTop = (uintptr_t)(stackBottom + co->stkMem.size);
    auto unwinder = std::make_shared<Unwinder>();
    auto regs = DfxRegs::CreateFromUcontext(ctx);
    unwinder->SetRegs(regs);
    UnwindContext context;
    context.pid = UNWIND_TYPE_LOCAL;
    context.regs = regs;
    context.maps = unwinder->GetMaps();
    context.stackCheck = false;
    context.stackBottom = stackBottom;
    context.stackTop = stackTop;
    bool resFlag = unwinder->Unwind(&context);
    if (!resFlag) {
        FFRT_LOGE("Call Unwind failed");
        return;
    }
    std::ostringstream ss;
    auto frames = unwinder->GetFrames();
    if (flag != 0) {
        ss << Unwinder::GetFramesStr(frames);
        ss << std::endl;
        stackInfo = ss.str();
        return;
    }
    FFRT_LOGE("%s", Unwinder::GetFramesStr(frames).c_str());
}
#endif
} /* namespace ffrt */