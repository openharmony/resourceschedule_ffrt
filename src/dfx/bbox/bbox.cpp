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
#include "dfx/trace_record/ffrt_trace_record.h"
#include "sched/scheduler.h"
#include "tm/queue_task.h"
#include "queue/queue_monitor.h"
#include "tm/task_factory.h"
#include "eu/cpuworker_manager.h"
#include "util/time_format.h"
#ifdef OHOS_STANDARD_SYSTEM
#include "dfx/bbox/fault_logger_fd_manager.h"
#endif
#include "dfx/dump/dump.h"
#include "util/ffrt_facade.h"
#include "util/slab.h"

using namespace ffrt;

constexpr static size_t EACH_QUEUE_TASK_DUMP_SIZE = 64;
static std::atomic<unsigned int> g_taskPendingCounter(0);
static std::atomic<unsigned int> g_taskWakeCounter(0);
static CPUEUTask* g_cur_task;
static unsigned int g_cur_tid;
static const char* g_cur_signame;
std::mutex bbox_handle_lock;
std::condition_variable bbox_handle_end;

static struct sigaction s_oldSa[SIGSYS + 1]; // SIGSYS = 31

static FuncSaveKeyStatusInfo saveKeyStatusInfo = nullptr;
static FuncSaveKeyStatus saveKeyStatus = nullptr;
void SetFuncSaveKeyStatus(FuncSaveKeyStatus func, FuncSaveKeyStatusInfo infoFunc)
{
    saveKeyStatus = func;
    saveKeyStatusInfo = infoFunc;
}

void TaskWakeCounterInc(void)
{
    ++g_taskWakeCounter;
}

void TaskPendingCounterInc(void)
{
    ++g_taskPendingCounter;
}

static inline void SaveCurrent()
{
    FFRT_BBOX_LOG("<<<=== current status ===>>>");
    auto t = g_cur_task;
    if (t) {
        if (t->type == ffrt_normal_task || t->type == ffrt_queue_task) {
            FFRT_BBOX_LOG("signal %s triggered: source tid %d, task id %lu, qos %d, name %s",
                g_cur_signame, g_cur_tid, t->gid, t->qos(), t->label.c_str());
        }
    }
}

#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_2)
static inline void SaveTaskCounter()
{
    FFRT_BBOX_LOG("<<<=== task counter ===>>>");
    FFRT_BBOX_LOG("FFRT BBOX TaskSubmitCounter:%u TaskEnQueueCounter:%u TaskDoneCounter:%u",
        FFRTTraceRecord::GetSubmitCount(), FFRTTraceRecord::GetEnqueueCount(), FFRTTraceRecord::GetDoneCount());
    FFRT_BBOX_LOG("FFRT BBOX TaskRunCounter:%u TaskSwitchCounter:%u TaskFinishCounter:%u",
        FFRTTraceRecord::GetRunCount(), FFRTTraceRecord::GetCoSwitchCount(), FFRTTraceRecord::GetFinishCount());
    FFRT_BBOX_LOG("FFRT BBOX TaskWakeCounterInc:%u, TaskPendingCounter:%u",
        g_taskWakeCounter.load(), g_taskPendingCounter.load());
    if (FFRTTraceRecord::GetCoSwitchCount() + FFRTTraceRecord::GetFinishCount() == FFRTTraceRecord::GetRunCount()) {
        FFRT_BBOX_LOG("TaskRunCounter equals TaskSwitchCounter + TaskFinishCounter");
    } else {
        FFRT_BBOX_LOG("TaskRunCounter is not equal to TaskSwitchCounter + TaskFinishCounter");
    }
}
#endif

static inline void SaveLocalFifoStatus(int qos, WorkerThread* thread)
{
    CPUWorker* worker = reinterpret_cast<CPUWorker*>(thread);
    CPUEUTask* t = reinterpret_cast<CPUEUTask*>(worker->localFifo.PopHead());
    while (t != nullptr) {
        if (t->type == ffrt_normal_task || t->type == ffrt_queue_task) {
            FFRT_BBOX_LOG("qos %d: worker tid %d is localFifo task id %lu name %s",
                qos, worker->Id(), t->gid, t->label.c_str());
        }
        t = reinterpret_cast<CPUEUTask*>(worker->localFifo.PopHead());
    }
}

static inline void SaveWorkerStatus()
{
    WorkerGroupCtl* workerGroup = FFRTFacade::GetEUInstance().GetGroupCtl();
    FFRT_BBOX_LOG("<<<=== worker status ===>>>");
    for (int i = 0; i < QoS::MaxNum(); i++) {
        std::shared_lock<std::shared_mutex> lck(workerGroup[i].tgMutex);
        for (auto& thread : workerGroup[i].threads) {
            SaveLocalFifoStatus(i, thread.first);
            CPUEUTask* t = thread.first->curTask;
            if (t == nullptr) {
                FFRT_BBOX_LOG("qos %d: worker tid %d is running nothing", i, thread.first->Id());
                continue;
            }
            if (t->type == ffrt_normal_task || t->type == ffrt_queue_task) {
                FFRT_BBOX_LOG("qos %d: worker tid %d is running task id %lu name %s", i, thread.first->Id(),
                    t->gid, t->label.c_str());
            }
        }
    }
}

static inline void SaveReadyQueueStatus()
{
    FFRT_BBOX_LOG("<<<=== ready queue status ===>>>");
    for (int i = 0; i < QoS::MaxNum(); i++) {
        int nt = FFRTFacade::GetSchedInstance()->GetScheduler(i).RQSize();
        if (!nt) {
            continue;
        }

        for (int j = 0; j < nt; j++) {
            CPUEUTask* t = FFRTFacade::GetSchedInstance()->GetScheduler(i).PickNextTask();
            if (t == nullptr) {
                FFRT_BBOX_LOG("qos %d: ready queue task <%d/%d> null", i, j, nt);
                continue;
            }
            if (t->type == ffrt_normal_task || t->type == ffrt_queue_task) {
                FFRT_BBOX_LOG("qos %d: ready queue task <%d/%d> id %lu name %s",
                    i, j, nt, t->gid, t->label.c_str());
            }
        }
    }
}

static inline void SaveKeyStatus()
{
    FFRT_BBOX_LOG("<<<=== key status ===>>>");
    if (saveKeyStatus == nullptr) {
        FFRT_BBOX_LOG("no key status");
        return;
    }
    saveKeyStatus();
}

static inline void SaveNormalTaskStatus()
{
    TaskFactory::LockMem();
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
            if (t->type == ffrt_normal_task) {
                FFRT_BBOX_LOG("<%zu/%lu> id %lu qos %d name %s", idx,
                    tmp.size(), t->gid, t->qos(), t->label.c_str());
                idx++;
            }
            if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH))
                && t != g_cur_task) {
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
                    std::string dumpInfo;
                    DumpTask(t, dumpInfo, 1);
#else
                    CoStart(t, GetCoEnv());
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
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
    TaskFactory::UnlockMem();
}

static inline void SaveQueueTaskStatus()
{
    std::lock_guard lk(SimpleAllocator<QueueTask>::Instance()->lock);
    auto unfreeQueueTask = SimpleAllocator<QueueTask>::getUnfreedMem();
    auto applyqueue = [&](const char* tag, const std::function<bool(QueueTask*)>& filter) {
        std::vector<QueueTask*> tmp;
        for (auto task : unfreeQueueTask) {
            auto t = reinterpret_cast<QueueTask*>(task);
            if (filter(t)) {
                tmp.emplace_back(t);
            }
        }

        if (tmp.size() > 0) {
            FFRT_BBOX_LOG("<<<=== %s ===>>>", tag);
        }
        size_t idx = 1;
        for (auto t : tmp) {
            if (t->type == ffrt_queue_task) {
                FFRT_BBOX_LOG("<%zu/%lu> id %lu qos %d name %s", idx,
                    tmp.size(), t->gid, t->GetQos(), t->label.c_str());
                idx++;
            }

            if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH))) {
                CoStart(reinterpret_cast<CPUEUTask*>(t), GetCoEnv());
            }
        }
    };

    applyqueue("queue task blocked by synchronization primitive(mutex etc)", [](QueueTask* t) {
        return (t->GetFinishStatus() == false) && t->coRoutine &&
            t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH);
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
    DumpTask(nullptr, dumpInfo, 1);
    if (!dumpInfo.empty()) {
        FFRT_BBOX_LOG("%s", dumpInfo.c_str());
    }
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
}

unsigned int GetBboxEnableState(void)
{
    return g_bbox_tid_is_dealing.load();
}

unsigned int GetBboxCalledTimes(void)
{
    return g_bbox_called_times.load();
}

bool FFRTIsWork()
{
    return FFRTTraceRecord::FfrtBeUsed();
}

void RecordDebugInfo(void)
{
    auto t = ExecuteCtx::Cur()->task;
    FFRT_BBOX_LOG("<<<=== ffrt debug log start ===>>>");

    if ((t != nullptr) && (t->type == ffrt_normal_task || t->type == ffrt_queue_task)) {
        FFRT_BBOX_LOG("debug log: tid %d, task id %lu, qos %d, name %s", gettid(), t->gid, t->qos(), t->label.c_str());
    }
    SaveKeyStatus();
    FFRT_BBOX_LOG("<<<=== ffrt debug log finish ===>>>");
}

void SaveTheBbox()
{
    if (g_bbox_called_times.fetch_add(1) == 0) { // only save once
        std::thread([&]() {
            unsigned int expect = 0;
            unsigned int tid = static_cast<unsigned int>(gettid());
            ffrt::CPUMonitor *monitor = ffrt::FFRTFacade::GetEUInstance().GetCPUMonitor();
            (void)g_bbox_tid_is_dealing.compare_exchange_strong(expect, tid);
            monitor->WorkerInit();

#ifdef OHOS_STANDARD_SYSTEM
            FaultLoggerFdManager::Instance().InitFaultLoggerFd();
#endif
            FFRT_BBOX_LOG("<<<=== ffrt black box(BBOX) start ===>>>");
            SaveCurrent();
#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_2)
            SaveTaskCounter();
#endif
            SaveWorkerStatus();
            SaveKeyStatus();
            SaveReadyQueueStatus();
            SaveNormalTaskStatus();
            SaveQueueTaskStatus();
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
        case SIGSTKFLT: return "SIGSTKFLT";
        case SIGSTOP: return "SIGSTOP";
        case SIGSYS: return "SIGSYS";
        case SIGTRAP: return "SIGTRAP";
        default: return "?";
    }
}

static void SignalHandler(int signo, siginfo_t* info, void* context __attribute__((unused)))
{
    if (FFRTIsWork()) {
        g_cur_task = ExecuteCtx::Cur()->task;
        g_cur_tid = gettid();
        g_cur_signame = GetSigName(info);
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
    SignalUnReg(SIGSTKFLT);
    SignalUnReg(SIGSYS);
    SignalUnReg(SIGTRAP);
    SignalUnReg(SIGINT);
    SignalUnReg(SIGKILL);
}

static inline std::string FormatDateString(uint64_t timeStamp)
{
#if defined(__aarch64__)
    return FormatDateString4CntCt(timeStamp, microsecond);
#else
    return FormatDateString4SteadyClock(timeStamp, microsecond);
#endif
}

std::string GetDumpPreface(void)
{
    std::ostringstream ss;
    ss << "|-> Launcher proc ffrt, now:" << FormatDateString(FFRTTraceRecord::TimeStamp()) << " pid:" << GetPid()
        << std::endl;
    return ss.str();
}

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_2)
std::string SaveTaskCounterInfo(void)
{
    std::ostringstream ss;
    ss << "    |-> task counter" << std::endl;
    ss << "        TaskSubmitCounter:" << FFRTTraceRecord::GetSubmitCount() << " TaskEnQueueCounter:"
       << FFRTTraceRecord::GetEnqueueCount() << " TaskDoneCounter:" << FFRTTraceRecord::GetDoneCount() << std::endl;

    ss << "        TaskRunCounter:" << FFRTTraceRecord::GetRunCount() << " TaskSwitchCounter:"
       << FFRTTraceRecord::GetCoSwitchCount() << " TaskFinishCounter:" << FFRTTraceRecord::GetFinishCount()
       << std::endl;

    if (FFRTTraceRecord::GetCoSwitchCount() + FFRTTraceRecord::GetFinishCount() == FFRTTraceRecord::GetRunCount()) {
        ss << "        TaskRunCounter equals TaskSwitchCounter + TaskFinishCounter" << std::endl;
    } else {
        ss << "        TaskRunCounter is not equal to TaskSwitchCounter + TaskFinishCounter" << std::endl;
    }
    return ss.str();
}
#endif // FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_2

void AppendTaskInfo(std::ostringstream& oss, TaskBase* task)
{
#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_1)
    if (task->fromTid) {
        oss << " fromTid " << task->fromTid;
    }
    if (task->createTime) {
        oss << " createTime " << FormatDateString(task->createTime);
    }
    if (task->executeTime) {
        oss << " executeTime " << FormatDateString(task->executeTime);
    }
#endif
}

std::string SaveKeyInfo(void)
{
    ffrt::CPUMonitor *monitor = ffrt::FFRTFacade::GetEUInstance().GetCPUMonitor();
    std::ostringstream oss;

    monitor->WorkerInit();
    oss << "    |-> key status" << std::endl;
    if (saveKeyStatusInfo == nullptr) {
        oss << "no key status info" << std::endl;
        return oss.str();
    }
    oss << saveKeyStatusInfo();
    return oss.str();
}

std::string SaveWorkerStatusInfo(void)
{
    std::ostringstream ss;
    std::ostringstream oss;
    WorkerGroupCtl* workerGroup = FFRTFacade::GetEUInstance().GetGroupCtl();
    oss << "    |-> worker count" << std::endl;
    ss << "    |-> worker status" << std::endl;
    for (int i = 0; i < QoS::MaxNum(); i++) {
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
            if (t->type == ffrt_normal_task || t->type == ffrt_queue_task) {
                ss << "        qos " << i << ": worker tid " << thread.first->Id()
                    << " is running, task id " << t->gid << " name " << t->label.c_str();
                AppendTaskInfo(ss, t);
                ss << std::endl;
            }
        }
        if (tidArr.size() == 0) {
            continue;
        }
        oss << "        qos " << i << ": worker num:" << tidArr.size() << " tid:";
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

std::string SaveNormalTaskStatusInfo(void)
{
    std::string ffrtStackInfo;
    std::ostringstream ss;
    TaskFactory::LockMem();
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
            if (t->type == ffrt_normal_task) {
                ss << "        <" << idx++ << "/" << tmp.size() << ">" << "stack: task id " << t->gid << ",qos "
                    << t->qos() << ",name " << t->label.c_str();
                AppendTaskInfo(ss, t);
                ss << std::endl;
            }
            ffrtStackInfo += ss.str();
            if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH))) {
                std::string dumpInfo;
                DumpTask(t, dumpInfo, 1);
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
    apply("ready task", [](CPUEUTask* t) {
        return t->state == TaskState::READY;
    });
    TaskFactory::UnlockMem();

    return ffrtStackInfo;
}

void DumpQueueTaskInfo(std::string& ffrtStackInfo, const char* tag, const std::vector<QueueTask*>& tasks,
    const std::function<bool(QueueTask*)>& filter, size_t limit = EACH_QUEUE_TASK_DUMP_SIZE)
{
    std::vector<QueueTask*> tmp;
    for (auto t : tasks) {
        if (tmp.size() < limit && filter(t)) {
            tmp.emplace_back(t);
        }
    }
    if (tmp.size() == 0) {
        return;
    }
    std::ostringstream ss;
    ss << "<<<=== " << tag << "===>>>" << std::endl;
    ffrtStackInfo += ss.str();

    size_t idx = 1;
    for (auto t : tmp) {
        ss.str("");
        if (t->type == ffrt_queue_task) {
            ss << "<" << idx++ << "/" << tmp.size() << ">" << "id " << t->gid << " qos "
                << t->GetQos() << " name " << t->label.c_str();
            AppendTaskInfo(ss, t);
            ss << std::endl;
        }
        ffrtStackInfo += ss.str();
        if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH))) {
            std::string dumpInfo;
            DumpTask(reinterpret_cast<CPUEUTask*>(t), dumpInfo, 1);
            ffrtStackInfo += dumpInfo;
        }
    }
}

std::string SaveQueueTaskStatusInfo()
{
    std::string ffrtStackInfo;
    std::lock_guard lk(SimpleAllocator<QueueTask>::Instance()->lock);
    auto unfreeQueueTask = SimpleAllocator<QueueTask>::getUnfreedMem();
    if (unfreeQueueTask.size() == 0) {
        return ffrtStackInfo;
    }

    std::map<QueueHandler*, std::vector<QueueTask*>> taskMap;
    for (auto t : unfreeQueueTask) {
        auto task = reinterpret_cast<QueueTask*>(t);
        if (task->type == ffrt_queue_task && task->GetFinishStatus() == false && task->GetHandler() != nullptr) {
            taskMap[task->GetHandler()].push_back(task);
        }
    }
    if (taskMap.empty()) {
        return ffrtStackInfo;
    }

    for (auto entry : taskMap) {
        std::sort(entry.second.begin(), entry.second.end(), [](QueueTask* first, QueueTask* second) {
            return first->GetUptime() < second->GetUptime();
        });
    }

    for (auto entry : taskMap) {
        ffrtStackInfo += "\n";
        DumpQueueTaskInfo(ffrtStackInfo, "queue task blocked by synchronization primitive(mutex etc)", entry.second,
            [](QueueTask* t) {
                return (t->GetFinishStatus() == false) && t->coRoutine &&
                    t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH);
            });
        DumpQueueTaskInfo(ffrtStackInfo, "queue task unFinished", entry.second, [](QueueTask* t) {
            return (t->GetFinishStatus() == false && !(t->coRoutine &&
                t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH)));
        });
    }

    return ffrtStackInfo;
}
#endif
#endif /* FFRT_BBOX_ENABLE */
