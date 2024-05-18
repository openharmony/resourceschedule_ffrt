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
#include <string>
#include <sstream>
#include <vector>
#include "dfx/log/ffrt_log_api.h"
#include "sched/scheduler.h"
#include "tm/task_factory.h"
#include "eu/cpuworker_manager.h"

using namespace ffrt;

static std::atomic<unsigned int> g_taskSubmitCounter(0);
static std::atomic<unsigned int> g_taskDoneCounter(0);
static std::atomic<unsigned int> g_taskEnQueueCounter(0);
static std::atomic<unsigned int> g_taskRunCounter(0);
static std::atomic<unsigned int> g_taskSwitchCounter(0);
static std::atomic<unsigned int> g_taskFinishCounter(0);
#ifdef FFRT_IO_TASK_SCHEDULER
static std::atomic<unsigned int> g_taskPendingCounter(0);
static std::atomic<unsigned int> g_taskWakeCounter(0);
#endif
static CPUEUTask* g_cur_task;
static unsigned int g_cur_tid;
static const char* g_cur_signame;
std::mutex bbox_handle_lock;
std::condition_variable bbox_handle_end;

static struct sigaction s_oldSa[SIGSYS + 1]; // SIGSYS = 31

void TaskSubmitCounterInc(void)
{
    ++g_taskSubmitCounter;
}

#ifdef FFRT_IO_TASK_SCHEDULER
void TaskWakeCounterInc(void)
{
    ++g_taskWakeCounter;
}
#endif

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

#ifdef FFRT_IO_TASK_SCHEDULER
void TaskPendingCounterInc(void)
{
    ++g_taskPendingCounter;
}
#endif

static inline void SaveCurrent()
{
    FFRT_BBOX_LOG("<<<=== current status ===>>>");
    auto t = g_cur_task;
    if (t) {
        if (t->type == 0) {
            FFRT_BBOX_LOG("signal %s triggered: source tid %d, task id %lu, qos %d, name %s",
                g_cur_signame, g_cur_tid, t->gid, t->qos(), t->label.c_str());
        }
    }
}

static inline void SaveTaskCounter()
{
    FFRT_BBOX_LOG("<<<=== task counter ===>>>");
    FFRT_BBOX_LOG("FFRT BBOX TaskSubmitCounter:%u TaskEnQueueCounter:%u TaskDoneCounter:%u",
        g_taskSubmitCounter.load(), g_taskEnQueueCounter.load(), g_taskDoneCounter.load());
    FFRT_BBOX_LOG("FFRT BBOX TaskRunCounter:%u TaskSwitchCounter:%u TaskFinishCounter:%u", g_taskRunCounter.load(),
        g_taskSwitchCounter.load(), g_taskFinishCounter.load());
#ifdef FFRT_IO_TASK_SCHEDULER
    FFRT_BBOX_LOG("FFRT BBOX TaskWakeCounterInc:%u, TaskPendingCounter:%u",
        g_taskWakeCounter.load(), g_taskPendingCounter.load());
#endif
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
    ffrt::QoS _qos = ffrt::QoS(static_cast<int>(qos_max));
    for (int i = 0; i < _qos() + 1; i++) {
        std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
        for (auto& thread : workerGroup[i].threads) {
            CPUEUTask* t = thread.first->curTask;
            if (t == nullptr) {
                FFRT_BBOX_LOG("qos %d: worker tid %d is running nothing", i, thread.first->Id());
                continue;
            }
            if (t->type == 0) {
                FFRT_BBOX_LOG("qos %d: worker tid %d is running task id %lu name %s", i, thread.first->Id(),
                    t->gid, t->label.c_str());
            }
        }
    }
}

static inline void SaveReadyQueueStatus()
{
    FFRT_BBOX_LOG("<<<=== ready queue status ===>>>");
    ffrt::QoS _qos = ffrt::QoS(static_cast<int>(qos_max));
    for (int i = 0; i < _qos() + 1; i++) {
        int nt = FFRTScheduler::Instance()->GetScheduler(QoS(i)).RQSize();
        if (!nt) {
            continue;
        }

        for (int j = 0; j < nt; j++) {
            CPUEUTask* t = FFRTScheduler::Instance()->GetScheduler(QoS(i)).PickNextTask();
            if (t == nullptr) {
                FFRT_BBOX_LOG("qos %d: ready queue task <%d/%d> null", i, j, nt);
                continue;
            }
            if (t->type == 0) {
                FFRT_BBOX_LOG("qos %d: ready queue task <%d/%d> id %lu name %s",
                    i, j, nt, t->gid, t->label.c_str());
            }
        }
    }
}

static inline void SaveTaskStatus()
{
    auto unfree = TaskFactory::GetUnfreedMem();
    auto apply = [&](const char* tag, const std::function<bool(CPUEUTask*)>& filter) {
        std::vector<CPUEUTask*> tmp;
        for (auto task : unfree) {
            auto t = reinterpret_cast<CPUEUTask*>(task);
            if (filter(t)) {
                tmp.emplace_back(t);
            }
        }

        if (tmp.size() > 0) {
            FFRT_BBOX_LOG("<<<=== %s ===>>>", tag);
        }
        size_t idx = 1;
        for (auto t : tmp) {
            if (t->type == 0) {
                FFRT_BBOX_LOG("<%zu/%lu> id %lu qos %d name %s", idx,
                    tmp.size(), t->gid, t->qos(), t->label.c_str());
                idx++;
            }
            if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH))
                && t != g_cur_task) {
                CoStart(t);
            }
        }
    };

    apply("blocked by synchronization primitive(mutex etc)", [](CPUEUTask* t) {
        return (t->state == TaskState::RUNNING) && t->coRoutine &&
            t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH) && t != g_cur_task;
    });
    apply("blocked by task dependence", [](CPUEUTask* t) {
        return t->state == TaskState::BLOCKED;
    });
    apply("pending task", [](CPUEUTask* t) {
        return t->state == TaskState::PENDING;
    });
}

static std::atomic_uint g_bbox_tid_is_dealing {0};
static std::atomic_uint g_bbox_called_times {0};
static std::condition_variable g_bbox_cv;
static std::mutex g_bbox_mtx;

void BboxFreeze()
{
    std::unique_lock<std::mutex> lk(g_bbox_mtx);
    g_bbox_cv.wait(lk, [] { return g_bbox_tid_is_dealing.load() == 0; });
}

void backtrace(int ignoreDepth)
{
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    std::string dumpInfo;
    CPUEUTask::DumpTask(nullptr, dumpInfo, 1);
    if (!dumpInfo.empty()) {
        FFRT_BBOX_LOG("%s", dumpInfo.c_str());
    }
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

unsigned int GetBboxEnableState(void)
{
    return g_bbox_tid_is_dealing.load();
}

bool FFRTIsWork()
{
    if (g_taskSubmitCounter.load() == 0) {
        return false;
    } else if (g_taskSubmitCounter.load() == g_taskDoneCounter.load()) {
        return false;
    }

    return true;
}

void SaveTheBbox()
{
    if (g_bbox_called_times.fetch_add(1) == 0) { // only save once
        std::thread([&]() {
            unsigned int expect = 0;
            unsigned int tid = static_cast<unsigned int>(gettid());
            (void)g_bbox_tid_is_dealing.compare_exchange_strong(expect, tid);

#ifdef OHOS_STANDARD_SYSTEM
            FaultLoggerFdManager::Instance().InitFaultLoggerFd();
#endif
            FFRT_BBOX_LOG("<<<=== ffrt black box(BBOX) start ===>>>");
            SaveCurrent();
            SaveTaskCounter();
            SaveWorkerStatus();
            SaveReadyQueueStatus();
            SaveTaskStatus();
            FFRT_BBOX_LOG("<<<=== ffrt black box(BBOX) finish ===>>>");
#ifdef OHOS_STANDARD_SYSTEM
            FaultLoggerFdManager::Instance().CloseFd();
#endif

            std::unique_lock handle_end_lk(bbox_handle_lock);
            bbox_handle_end.notify_one();

            std::lock_guard lk(g_bbox_mtx);
            g_bbox_tid_is_dealing.store(0);
            g_bbox_cv.notify_all();
        }).detach();

        {
            std::unique_lock lk(bbox_handle_lock);
            (void)bbox_handle_end.wait_for(lk, std::chrono::seconds(5));
        }
    } else {
        unsigned int tid = static_cast<unsigned int>(gettid());
        if (tid == g_bbox_tid_is_dealing.load()) {
            FFRT_LOGE("thread %u black box save failed", tid);
            g_bbox_tid_is_dealing.store(0);
            g_bbox_cv.notify_all();
        } else {
            FFRT_LOGE("thread %u trigger signal again, when thread %u is saving black box",
                tid, g_bbox_tid_is_dealing.load());
            BboxFreeze(); // hold other thread's signal resend
        }
    }
}

static void ResendSignal(siginfo_t* info)
{
    int rc = syscall(SYS_rt_tgsigqueueinfo, getpid(), syscall(SYS_gettid), info->si_signo, info);
    if (rc != 0) {
        FFRT_LOGE("ffrt failed to resend signal during crash");
    }
}

static const char* GetSigName(const siginfo_t* info)
{
    switch (info->si_signo) {
        case SIGABRT: return "SIGABRT";
        case SIGBUS: return "SIGBUS";
        case SIGFPE: return "SIGFPE";
        case SIGILL: return "SIGILL";
        case SIGSEGV: return "SIGSEGV";
        case SIGSTKFLT: return "SIGSTKFLT";
        case SIGSTOP: return "SIGSTOP";
        case SIGSYS: return "SIGSYS";
        case SIGTRAP: return "SIGTRAP";
        default: return "?";
    }
}

static void SignalHandler(int signo, siginfo_t* info, void* context __attribute__((unused)))
{
    g_cur_task = ExecuteCtx::Cur()->task;
    g_cur_tid = gettid();
    g_cur_signame = GetSigName(info);
    if (FFRTIsWork()) {
        SaveTheBbox();
    }
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

static void SignalUnReg(int signo)
{
    sigaction(signo, &s_oldSa[signo], nullptr);
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
    SignalReg(SIGINT);
    SignalReg(SIGKILL);
}

__attribute__((destructor)) static void BBoxDeInit()
{
    SignalUnReg(SIGABRT);
    SignalUnReg(SIGBUS);
    SignalUnReg(SIGFPE);
    SignalUnReg(SIGILL);
    SignalUnReg(SIGSEGV);
    SignalUnReg(SIGSTKFLT);
    SignalUnReg(SIGSYS);
    SignalUnReg(SIGTRAP);
    SignalUnReg(SIGINT);
    SignalUnReg(SIGKILL);
}

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
std::string SaveTaskCounterInfo(void)
{
    std::ostringstream ss;
    ss << "    |-> task counter" << std::endl;
    ss << "        FFRT BBOX TaskSubmitCounter:" << g_taskSubmitCounter.load() << " TaskEnQueueCounter:"
       << g_taskEnQueueCounter.load() << " TaskDoneCounter:" << g_taskDoneCounter.load() << std::endl;

    ss << "        FFRT BBOX TaskRunCounter:" << g_taskRunCounter.load() << " TaskSwitchCounter:"
       << g_taskSwitchCounter.load() << " TaskFinishCounter:" << g_taskFinishCounter.load() << std::endl;

    if (g_taskSwitchCounter.load() + g_taskFinishCounter.load() == g_taskRunCounter.load()) {
        ss << "        TaskRunCounter equals TaskSwitchCounter + TaskFinishCounter" << std::endl;
    } else {
        ss << "        TaskRunCounter is not equal to TaskSwitchCounter + TaskFinishCounter" << std::endl;
    }
    return ss.str();
}

std::string SaveWorkerStatusInfo(void)
{
    std::ostringstream ss;
    std::ostringstream oss;
    WorkerGroupCtl* workerGroup = ExecuteUnit::Instance().GetGroupCtl();
    oss << "    |-> worker count" << std::endl;
    ss << "    |-> worker status" << std::endl;
    ffrt::QoS _qos = ffrt::QoS(static_cast<int>(qos_max));
    for (int i = 0; i < _qos() + 1; i++) {
        std::vector<int> tidArr;
        std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
        for (auto& thread : workerGroup[i].threads) {
            CPUEUTask* t = thread.first->curTask;
            tidArr.push_back(thread.first->Id());
            if (t == nullptr) {
                ss << "        qos " << i << ": worker tid " << thread.first->Id()
                   << " is running nothing" << std::endl;
                continue;
            }

            if (t->type == 0) {
                ss << "        qos " << i << ": worker tid " << thread.first->Id()
                << " is running, task id " << t->gid << " name " << t->label.c_str() << std::endl;
            }
        }
        if (tidArr.size() == 0) {
            continue;
        }
        oss << "        qos " << i << ": worker num:" << tidArr.size() << "tid:";
        std::for_each(tidArr.begin(), tidArr.end(), [&](const int &t) {
            if (&t == &tidArr.back()) {
                oss << t;
            } else {
                oss << t << ", ";
            }
        });
        oss << std::endl;
    }
    oss << ss.str();
    return oss.str();
}

std::string SaveReadyQueueStatusInfo()
{
    std::ostringstream ss;
    ss << "    |-> ready queue status" << std::endl;
    ffrt::QoS _qos = ffrt::QoS(static_cast<int>(qos_max));
    for (int i = 0; i < _qos() + 1; i++) {
        auto lock = ExecuteUnit::Instance().GetSleepCtl(static_cast<int>(QoS(i)));
        std::lock_guard lg(*lock);

        int nt = FFRTScheduler::Instance()->GetScheduler(QoS(i)).RQSize();
        if (!nt) {
            continue;
        }

        for (int j = 1; j <= nt; j++) {
            CPUEUTask* t = FFRTScheduler::Instance()->GetScheduler(QoS(i)).PickNextTask();
            if (t == nullptr) {
                ss << "        qos " << i << ": ready queue task <" << j << "/" << nt << ">"
                   << " null" << std::endl;
                continue;
            }
            if (t->type == 0) {
                ss << "        qos " << i << ": ready queue task <" << j << "/" << nt << "> task id "
                << t->gid << " name " << t->label.c_str() << std::endl;
            }

            FFRTScheduler::Instance()->GetScheduler(QoS(i)).WakeupTask(t);
        }
    }
    return ss.str();
}

std::string SaveTaskStatusInfo(void)
{
    std::string ffrtStackInfo;
    std::ostringstream ss;
    auto unfree = TaskFactory::GetUnfreedMem();
    auto apply = [&](const char* tag, const std::function<bool(CPUEUTask*)>& filter) {
        std::vector<CPUEUTask*> tmp;
        for (auto task : unfree) {
            auto t = reinterpret_cast<CPUEUTask*>(task);
            if (filter(t)) {
                tmp.emplace_back(reinterpret_cast<CPUEUTask*>(t));
            }
        }

        if (tmp.size() > 0) {
            ss << "    |-> " << tag << std::endl;
            ffrtStackInfo += ss.str();
        }
        size_t idx = 1;
        for (auto t : tmp) {
            ss.str("");
            if (t->type == 0) {
                ss << "        <" << idx++ << "/" << tmp.size() << ">" << "stack: task id" << t->gid << "qos"
                << t->qos() << ",name " << t->label.c_str() << std::endl;
            }
            ffrtStackInfo += ss.str();
            if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH))) {
                std::string dumpInfo;
                CPUEUTask::DumpTask(t, dumpInfo, 1);
                ffrtStackInfo += dumpInfo;
            }
        }
    };

    apply("blocked by synchronization primitive(mutex etc)", [](CPUEUTask* t) {
        return (t->state == TaskState::RUNNING) && t->coRoutine &&
            t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH);
    });
    apply("blocked by task dependence", [](CPUEUTask* t) {
        return t->state == TaskState::BLOCKED;
    });
    apply("pending task", [](CPUEUTask* t) {
        return t->state == TaskState::PENDING;
    });

    return ffrtStackInfo;
}
#endif
#endif /* FFRT_BBOX_ENABLE */