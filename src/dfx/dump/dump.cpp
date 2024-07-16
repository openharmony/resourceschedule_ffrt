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
#include "dump.h"
#include <securec.h>
#include "c/ffrt_dump.h"
#include "dfx/bbox/bbox.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
using namespace OHOS::HiviewDFX;
#endif

namespace ffrt {
constexpr uint32_t DEFAULT_TIMEOUT_MS = 30000;
struct TimeoutCfg {
    static inline TimeoutCfg* Instance()
    {
        static TimeoutCfg inst;
        return &inst;
    }

    uint32_t timeout = DEFAULT_TIMEOUT_MS;
    ffrt_task_timeout_cb callback = nullptr;
};

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
void DumpTask(CPUEUTask* task, std::string& stackInfo, uint8_t flag)
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
    uintptr_t stackBottom = reinterpret_cast<uintptr_t>(reinterpret_cast<char*>(co) + sizeof(CoRoutine) - 8);
    uintptr_t stackTop = static_cast<uintptr_t>(stackBottom + co->stkMem.size);
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
}

#ifdef __cplusplus
extern "C" {
#endif //__cplusplus

API_ATTRIBUTE((visibility("default")))
int dump_info_all(char *buf, uint32_t len)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    if (FFRTIsWork()) {
        std::string dumpInfo;
        dumpInfo += "|-> Launcher proc ffrt, pid:" + std::to_string(GetPid()) + "\n";
        dumpInfo += SaveTaskCounterInfo();
        dumpInfo += SaveWorkerStatusInfo();
        dumpInfo += SaveReadyQueueStatusInfo();
        dumpInfo += SaveTaskStatusInfo();
        if (dumpInfo.length() > (len - 1)) {
            FFRT_LOGW("dumpInfo exceeds the buffer length, length:%d", dumpInfo.length());
        }
        int printed_num = snprintf_s(buf, len, len - 1, "%s", dumpInfo.c_str());
        if (printed_num == -1) {
            return snprintf_s(buf, len, len - 1, "|-> watchdog fail to print dumpinfo, pid: %s\n",
                std::to_string(GetPid()).c_str());
        }
        return printed_num;
    } else {
        return snprintf_s(buf, len, len - 1, "|-> FFRT has done all tasks, pid: %s\n",
            std::to_string(GetPid()).c_str());
    }
#else
    return -1;
#endif
}

API_ATTRIBUTE((visibility("default")))
int ffrt_dump(ffrt_dump_cmd_t cmd, char *buf, uint32_t len)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    switch (static_cast<ffrt_dump_cmd_t>(cmd)) {
        case ffrt_dump_cmd_t::DUMP_INFO_ALL: {
            return dump_info_all(buf, len);
        }
        default: {
            FFRT_LOGE("ffr_dump unsupport cmd[%d]", cmd);
        }
    }
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
    return -1;
}

API_ATTRIBUTE((visibility("default")))
ffrt_task_timeout_cb ffrt_task_timeout_get_cb(void)
{
    return ffrt::TimeoutCfg::Instance()->callback;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_timeout_set_cb(ffrt_task_timeout_cb cb)
{
    ffrt::TimeoutCfg::Instance()->callback = cb;
}

API_ATTRIBUTE((visibility("default")))
uint32_t ffrt_task_timeout_get_threshold(void)
{
    return ffrt::TimeoutCfg::Instance()->timeout;
}

API_ATTRIBUTE((visibility("default")))
void ffrt_task_timeout_set_threshold(uint32_t threshold_ms)
{
    ffrt::TimeoutCfg::Instance()->timeout = threshold_ms;
}
#ifdef __cplusplus
}
#endif