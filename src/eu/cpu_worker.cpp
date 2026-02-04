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

#include "eu/cpu_worker.h"
#include <algorithm>
#include <sched.h>
#include <sys/syscall.h>
#include "dm/dependence_manager.h"
#include "dfx/perf/ffrt_perf.h"
#include "tm/queue_task.h"
#include "eu/execute_unit.h"
#include "dfx/sysevent/sysevent.h"
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
#include "eu/blockaware.h"
#endif
#include "util/capability.h"
#include "util/ffrt_facade.h"
#include "util/white_list.h"
#ifdef OHOS_THREAD_STACK_DUMP
#include "dfx_dump_catcher.h"
#endif
namespace {
const unsigned int TRY_POLL_FREQ = 51;
const unsigned int LOCAL_QUEUE_SIZE = 128;
const unsigned int STEAL_BUFFER_SIZE = LOCAL_QUEUE_SIZE / 2;
}

namespace ffrt {
CPUWorker::CPUWorker(const QoS& qos, CpuWorkerOps&& ops, size_t stackSize) : qos(qos), ops(std::move(ops))
{
#ifdef FFRT_PTHREAD_ENABLE
    pthread_attr_init(&attr_);
    if (stackSize > 0) {
        pthread_attr_setstacksize(&attr_, stackSize);
    }
    SetThreadInitPri();
#endif
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    domain_id = (qos() <= BLOCKAWARE_DOMAIN_ID_MAX) ? qos() : BLOCKAWARE_DOMAIN_ID_MAX + 1;
#endif
#ifdef FFRT_SEND_EVENT
    uint64_t freq = 1000000;
    isBetaVersion = GetBetaVersionFlag();
#if defined(__aarch64__)
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
#endif
    this->cacheFreq = freq;
    this->cacheQos = static_cast<int>(qos);
#endif
    FFRT_TRACE_SCOPE(1, Start);
#ifdef FFRT_PTHREAD_ENABLE
    Start(CPUWorker::WrapDispatch, this);
#else
    Start(CPUWorker::Dispatch, this);
#endif
}

CPUWorker::~CPUWorker()
{
    if (!exited) {
#ifdef OHOS_THREAD_STACK_DUMP
        FFRT_LOGW("CPUWorker enter destruction but not exited");
        OHOS::HiviewDFX::DfxDumpCatcher dumplog;
        std::string msg = "";
        bool result = dumplog.DumpCatch(getpid(), gettid(), msg);
        if (result) {
            std::vector<std::string> out;
            std::stringstream ss(msg);
            std::string s;
            while (std::getline(ss, s, '\n')) {
                out.push_back(s);
            }
            for (auto const& line: out) {
                FFRT_LOGE("ffrt callstack %s", line.c_str());
            }
        }
#endif
    }
    Detach();
}

#ifdef FFRT_PTHREAD_ENABLE
void CPUWorker::SetThreadInitPri()
{
    bool enable = WhiteList::GetInstance().IsEnabled("SetThreadInitPri", false);
    if (!enable) {
        return;
    }

    if (qos() != GetFuncQosMax()() - 1) {
        return;
    }

    bool check = CheckProcCapSysNice();
    if (!check) {
        return;
    }

    (void)pthread_attr_setinheritsched(&attr_, PTHREAD_EXPLICIT_SCHED);
    (void)pthread_attr_setschedpolicy(&attr_, SCHED_RR);
    struct sched_param param {};
    param.sched_priority = 1;
    (void)pthread_attr_setschedparam(&attr_, &param);
}
#endif

void CPUWorker::NativeConfig()
{
    pid_t pid = syscall(SYS_gettid);
    this->tid = pid;
    SetThreadAttr(qos);
}

void* CPUWorker::WrapDispatch(void* worker)
{
    reinterpret_cast<CPUWorker*>(worker)->NativeConfig();
    Dispatch(reinterpret_cast<CPUWorker*>(worker));
    return nullptr;
}

void CPUWorker::RunTask(TaskBase* task, CPUWorker* worker)
{
    bool isNotUv = task->type == ffrt_normal_task || task->type == ffrt_queue_task;
#ifdef FFRT_SEND_EVENT
    uint64_t startExecuteTime = 0;
    if (worker->isBetaVersion) {
        startExecuteTime = TimeStampCntvct();
        if (likely(isNotUv)) {
            worker->cacheLabel = task->GetLabel();
        }
    }
#endif
    worker->curTask.store(task, std::memory_order_relaxed);
    worker->curTaskType_.store(task->type, std::memory_order_relaxed);
#ifdef WORKER_CACHE_TASKNAMEID
    if (isNotUv) {
        worker->curTaskLabel_ = task->GetLabel();
        worker->curTaskGid_ = task->gid;
    }
#endif

    ExecuteTask(task);

    worker->curTask.store(nullptr, std::memory_order_relaxed);
    worker->curTaskType_.store(ffrt_invalid_task, std::memory_order_relaxed);
#ifdef FFRT_SEND_EVENT
    if (worker->isBetaVersion) {
        uint64_t execDur = ((TimeStampCntvct() - startExecuteTime) / worker->cacheFreq);
        TaskBlockInfoReport(execDur, isNotUv ? worker->cacheLabel : "uv_task", worker->cacheQos, worker->cacheFreq);
    }
#endif
}

void CPUWorker::Dispatch(CPUWorker* worker)
{
    QoS qos = worker->GetQos();
    FFRTFacade::GetEUInstance().WorkerStart(qos());
    worker->WorkerSetup();

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    if (worker->ops.IsBlockAwareInit()) {
        int ret = BlockawareRegister(worker->GetDomainId());
        if (ret != 0) {
            FFRT_SYSEVENT_LOGE("blockaware register fail, ret[%d]", ret);
        }
    }
#endif
    auto ctx = ExecuteCtx::Cur();
    ctx->qos = qos;
    ctx->threadType_ = ffrt::ThreadType::FFRT_WORKER;
    *(FFRTFacade::GetSchedInstance()->GetScheduler(qos).GetWorkerTick()) = &(worker->tick);

    worker->ops.WorkerPrepare(worker);
#ifndef OHOS_STANDARD_SYSTEM
    FFRT_LOGI("qos[%d] thread start succ", static_cast<int>(qos));
#endif
    FFRT_PERF_WORKER_AWAKE(static_cast<int>(qos));
    WorkerLooper(worker);
    CoWorkerExit();
    FFRTFacade::GetEUInstance().WorkerExit(qos());
    worker->ops.WorkerRetired(worker);
}

bool CPUWorker::RunSingleTask(int qos, CPUWorker *worker)
{
    TaskBase *task = FFRTFacade::GetSchedInstance()->PopTask(qos);
    if (task) {
        RunTask(task, worker);
        return true;
    }
    return false;
}

// work looper which inherited from history
void CPUWorker::WorkerLooper(CPUWorker* worker)
{
    QoS qos = worker->GetQos();
    for (;;) {
        if (worker->Exited()) {
            break;
        }

        TaskBase* task = Scheduler::Instance()->PopTask(qos);
        worker->tick++;
        if (task) {
            if (Scheduler::Instance()->GetTaskSchedMode(qos) ==
                TaskSchedMode::DEFAULT_TASK_SCHED_MODE) {
                FFRTFacade::GetEUInstance().NotifyTask<TaskNotifyType::TASK_PICKED>(qos);
            }
            RunTask(task, worker);
            continue;
        }
        // It is about to enter the idle state.
        if (FFRTFacade::GetEUInstance().GetSchedMode(qos) == sched_mode_type::sched_energy_saving_mode) {
            if (FFRTFacade::GetEUInstance().WorkerShare(worker, RunSingleTask)) {
                continue;
            }
        }
        FFRT_PERF_WORKER_IDLE(static_cast<int>(worker->qos));
        auto action = worker->ops.WorkerIdleAction(worker);
        if (action == WorkerAction::RETRY) {
            FFRT_PERF_WORKER_AWAKE(static_cast<int>(worker->qos));
            worker->tick = 0;
            continue;
        } else if (action == WorkerAction::RETIRE) {
            break;
        }
    }
}
} // namespace ffrt
