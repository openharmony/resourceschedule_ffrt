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
#ifdef FFRT_BBOX_ENABLE

#include "bbox.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#ifdef FFRT_CO_BACKTRACE_ENABLE
#include <utils/CallStack.h>
#endif
#include "dfx/log/ffrt_log_api.h"
#include "sched/scheduler.h"

static std::atomic<unsigned int> g_taskSubmitCounter(0);
static std::atomic<unsigned int> g_taskDoneCounter(0);
static std::atomic<unsigned int> g_taskEnQueueCounter(0);
static std::atomic<unsigned int> g_taskRunCounter(0);
static std::atomic<unsigned int> g_taskSwitchCounter(0);
static std::atomic<unsigned int> g_taskFinishCounter(0);

static struct sigaction s_oldSa[SIGSYS + 1]; // SIGSYS = 31

using namespace ffrt;

void TaskSubmitCounterInc(void)
{
    ++g_taskSubmitCounter;
}

void TaskDoneCounterInc(void)
{
    ++g_taskDoneCounter;
}

void TaskEnQueuCounterInc(void)
{
    ++g_taskEnQueueCounter;
}

void TaskRunCounterInc(void)
{
    ++g_taskRunCounter;
}

void TaskSwitchCounterInc(void)
{
    ++g_taskSwitchCounter;
}

void TaskFinishCounterInc(void)
{
    ++g_taskFinishCounter;
}

static inline void SaveCurrent()
{
    FFRT_BBOX_LOG("<<<=== current status ===>>>");
    auto t = ExecuteCtx::Cur()->task;
    if (t) {
        FFRT_BBOX_LOG("current task id %lu qos %d name %s",
            t->gid, t->qos(), t->label.c_str());
    }

    const int IGNORE_DEPTH = 3;
    backtrace(IGNORE_DEPTH);
}

static inline void SaveTaskCounter()
{
    FFRT_BBOX_LOG("<<<=== task counter ===>>>");
    FFRT_BBOX_LOG("FFRT BBOX TaskSubmitCounter:%u TaskEnQueueCounter:%u TaskDoneCounter:%u",
        g_taskSubmitCounter.load(), g_taskEnQueueCounter.load(), g_taskDoneCounter.load());
    FFRT_BBOX_LOG("FFRT BBOX TaskRunCounter:%u TaskSwitchCounter:%u TaskFinishCounter:%u", g_taskRunCounter.load(),
        g_taskSwitchCounter.load(), g_taskFinishCounter.load());
    if (g_taskSwitchCounter.load() + g_taskFinishCounter.load() == g_taskRunCounter.load()) {
        FFRT_BBOX_LOG("TaskRunCounter equals TaskSwitchCounter + TaskFinishCounter");
    } else {
        FFRT_BBOX_LOG("TaskRunCounter is not equal to TaskSwitchCounter + TaskFinishCounter");
    }
}
static inline void SaveWorkerStatus()
{
    WorkerGroupCtl* workerGroup = ExecuteUnit::Instance().GetGroupCtl();
    FFRT_BBOX_LOG("<<<=== worker status ===>>>");
    for (int i = 0; i < qos_user_interactive + 1; i++) {
        std::unique_lock lock(workerGroup[i].tgMutex);
        for (auto& thread : workerGroup[i].threads) {
            TaskCtx* t = thread.first->curTask;
            if (t == nullptr) {
                FFRT_BBOX_LOG("qos %d: worker tid %d is running nothing", i, thread.first->Id());
                continue;
            }
            FFRT_BBOX_LOG("qos %d: worker tid %d is running task id %lu name %s", i, thread.first->Id(),
                t->gid, t->label.c_str());
        }
    }
}

static inline void SaveReadyQueueStatus()
{
    FFRT_BBOX_LOG("<<<=== ready queue status ===>>>");
    for (int i = 0; i < qos_user_interactive + 1; i++) {
        int nt = FFRTScheduler::Instance()->GetScheduler(QoS(static_cast<enum qos>(i))).RQSize();
        if (!nt) {
            continue;
        }

        for (int j = 0; j < nt; j++) {
            TaskCtx* t = FFRTScheduler::Instance()->GetScheduler(QoS(static_cast<enum qos>(i))).PickNextTask();
            if (t == nullptr) {
                FFRT_BBOX_LOG("qos %d: ready queue task <%d/%d> null", i + 1, j, nt);
                continue;
            }
            FFRT_BBOX_LOG("qos %d: ready queue task <%d/%d> id %lu name %s",
                i + 1, j, nt, t->gid, t->label.c_str());
        }
    }
}

static inline void SaveTaskStatus()
{
    auto unfree = SimpleAllocator<TaskCtx>::getUnfreedMem();
    auto apply = [&](const char* tag, const std::function<bool(TaskCtx*)>& filter) {
        decltype(unfree) tmp;
        for (auto t : unfree) {
            if (filter(t)) {
                tmp.emplace_back(t);
            }
        }

        if (tmp.size() > 0) {
            FFRT_BBOX_LOG("<<<=== %s ===>>>", tag);
        }
        size_t idx = 1;
        for (auto t : tmp) {
            FFRT_BBOX_LOG("<%zu/%lu> id %lu qos %d name %s", idx++,
                tmp.size(), t->gid, t->qos(), t->label.c_str());
            if (t->coRoutine && (t->coRoutine->status == static_cast<int>(CoStatus::CO_NOT_FINISH))) {
                CoStart(t);
            }
        }
    };

    apply("block by co task", [](TaskCtx* t) {
        return (t->state == TaskState::RUNNING) && t->coRoutine &&
            t->coRoutine->status == static_cast<int>(CoStatus::CO_NOT_FINISH);
    });
    apply("block by deps task", [](TaskCtx* t) {
        return t->state == TaskState::BLOCKED;
    });
    apply("pending task", [](TaskCtx* t) {
        return t->state == TaskState::PENDING;
    });
}

static std::condition_variable g_bbox_cv;
static std::mutex g_bbox_mtx;

void BboxFreeze()
{
    std::unique_lock<std::mutex> lk(g_bbox_mtx);
    g_bbox_cv.wait(lk);
}

void backtrace(int ignoreDepth)
{
    FFRT_BBOX_LOG("backtrace");

}

static std::atomic_int g_bbox_is_enable {0};

int GetBboxEnableState(void)
{
    return g_bbox_is_enable.load();
}

static void SaveTheBbox()
{
    if (g_bbox_is_enable.fetch_add(1) != 0) {
        return;
    }
    FFRT_BBOX_LOG("<<<=== ffrt black box start ===>>>");
    SaveCurrent();
    SaveTaskCounter();
    SaveWorkerStatus();
    SaveReadyQueueStatus();
    SaveTaskStatus();
    FFRT_BBOX_LOG("<<<=== ffrt black box finish ===>>>");
}

static void ResendSignal(siginfo_t* info)
{
    int rc = syscall(SYS_rt_tgsigqueueinfo, getpid(), syscall(SYS_gettid), info->si_signo, info);
    if (rc != 0) {
        FFRT_BBOX_LOG("ffrt failed to resend signal during crash");
    }
}

static void SignalHandler(int signo, siginfo_t* info, void* context __attribute__((unused)))
{
    SaveTheBbox();

    // we need to deregister our signal handler for that signal before continuing.
    sigaction(signo, &s_oldSa[signo], nullptr);
    ResendSignal(info);
}

static void SignalReg(int signo)
{
    sigaction(signo, nullptr, &s_oldSa[signo]);
    struct sigaction newAction;
    newAction.sa_flags = SA_RESTART | SA_SIGINFO;
    newAction.sa_sigaction = SignalHandler;
    sigaction(signo, &newAction, nullptr);
}

__attribute__((constructor)) static void BBoxInit()
{
    SignalReg(SIGABRT);
    SignalReg(SIGBUS);
    SignalReg(SIGFPE);
    SignalReg(SIGILL);
    SignalReg(SIGSEGV);
    SignalReg(SIGSTKFLT);
    SignalReg(SIGSYS);
    SignalReg(SIGTRAP);
}

#endif /* FFRT_BBOX_ENABLE */