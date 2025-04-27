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
#include <sys/wait.h>
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
#include "queue/traffic_record.h"
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
constexpr static unsigned int WAIT_PID_SLEEP_MS = 2;
constexpr static unsigned int WAIT_PID_MAX_RETRIES = 1000;
static std::atomic<unsigned int> g_taskPendingCounter(0);
static std::atomic<unsigned int> g_taskWakeCounter(0);
static CPUEUTask* g_cur_task;
static unsigned int g_cur_pid;
static unsigned int g_cur_tid;
static const char* g_cur_signame;

static struct sigaction s_oldSa[SIGSYS + 1]; // SIGSYS = 31

static FuncSaveKeyStatusInfo saveKeyStatusInfo = nullptr;
static FuncSaveKeyStatus saveKeyStatus = nullptr;
static FuncGetKeyStatus getKeyStatus = nullptr;
void SetFuncSaveKeyStatus(FuncGetKeyStatus getFunc, FuncSaveKeyStatus saveFunc, FuncSaveKeyStatusInfo infoFunc)
{
    getKeyStatus = getFunc;
    saveKeyStatus = saveFunc;
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

static void SignalUnReg(int signo)
{
    sigaction(signo, &s_oldSa[signo], nullptr);
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

static inline void SaveCurrent()
{
    FFRT_BBOX_LOG("<<<=== current status ===>>>");
    FFRT_BBOX_LOG("signal %s triggered: source pid %d, tid %d", g_cur_signame, g_cur_pid, g_cur_tid);
    auto t = g_cur_task;
    if (t) {
        if (t->type == ffrt_normal_task || t->type == ffrt_queue_task) {
            FFRT_BBOX_LOG("task id %lu, qos %d, name %s", t->gid, t->qos_(), t->label.c_str());
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
        for (auto& thread : workerGroup[i].threads) {
            SaveLocalFifoStatus(i, thread.first);
            TaskBase* t = thread.first->curTask;
            if (t == nullptr) {
                FFRT_BBOX_LOG("qos %d: worker tid %d is running nothing", i, thread.first->Id());
                continue;
            }
            FFRT_BBOX_LOG("qos %d: worker tid %d is running task", i, thread.first->Id());
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
    auto unfree = TaskFactory<CPUEUTask>::GetUnfreedMem();
    auto apply = [&](const char* tag, const std::function<bool(CPUEUTask*)>& filter) {
        std::vector<CPUEUTask*> tmp;
        for (auto task : unfree) {
            auto t = reinterpret_cast<CPUEUTask*>(task);
            auto f = reinterpret_cast<ffrt_function_header_t*>(t->func_storage);
            if (((f->reserve[0] & MASK_FOR_HCS_TASK) != MASK_FOR_HCS_TASK) && filter(t)) {
                tmp.emplace_back(t);
            }
        }

        if (tmp.size() > 0) {
            FFRT_BBOX_LOG("<<<=== %s ===>>>", tag);
        }
        size_t idx = 1;
        for (auto t : tmp) {
            if (t->type == ffrt_normal_task) {
                FFRT_BBOX_LOG("<%zu/%lu> id %lu qos %d name %s", idx, tmp.size(), t->gid, t->qos_(), t->label.c_str());
                idx++;
            }
            if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH)) &&
                t != g_cur_task) {
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
                std::string dumpInfo;
                DumpTask(t, dumpInfo, 1);
                if (!dumpInfo.empty()) {
                    FFRT_BBOX_LOG("%s", dumpInfo.c_str());
                }
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
    apply("ready task", [](CPUEUTask* t) {
        return t->state == TaskState::READY;
    });
}

static void DumpQueueTask(const char* tag, const std::vector<QueueTask*>& tasks,
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

    FFRT_BBOX_LOG("<<<=== %s ===>>>", tag);
    size_t idx = 1;
    for (auto t : tmp) {
        if (t->type == ffrt_queue_task) {
            FFRT_BBOX_LOG("<%zu/%lu> id %lu qos %d name %s", idx, tmp.size(), t->gid, t->GetQos(), t->label.c_str());
            idx++;
        }
        if (t->coRoutine && (t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH))) {
#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
            std::string dumpInfo;
            DumpTask(reinterpret_cast<CPUEUTask*>(t), dumpInfo, 1);
            if (!dumpInfo.empty()) {
                FFRT_BBOX_LOG("%s", dumpInfo.c_str());
            }
#else
            CoStart(reinterpret_cast<CPUEUTask*>(t), GetCoEnv());
#endif // FFRT_CO_BACKTRACE_OH_ENABLE
        }
    }
}

static inline void SaveQueueTaskStatus()
{
    auto unfreeQueueTask = SimpleAllocator<QueueTask>::getUnfreedMem();
    if (unfreeQueueTask.size() == 0) {
        return;
    }

    std::map<QueueHandler*, std::vector<QueueTask*>> taskMap;
    for (auto t : unfreeQueueTask) {
        auto task = reinterpret_cast<QueueTask*>(t);
        if (task->type == ffrt_queue_task && task->GetFinishStatus() == false && task->GetHandler() != nullptr) {
            taskMap[task->GetHandler()].push_back(task);
        }
    }
    if (taskMap.empty()) {
        return;
    }

    for (auto entry : taskMap) {
        std::sort(entry.second.begin(), entry.second.end(), [](QueueTask* first, QueueTask* second) {
            return first->GetUptime() < second->GetUptime();
        });
    }

    for (auto entry : taskMap) {
        DumpQueueTask("queue task blocked by synchronization primitive(mutex etc)", entry.second,
            [](QueueTask* t) {
                return (t->GetFinishStatus() == false) && t->coRoutine &&
                    t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH);
            });
        DumpQueueTask("queue task unFinished", entry.second, [](QueueTask* t) {
            return (t->GetFinishStatus() == false && !(t->coRoutine &&
                t->coRoutine->status.load() == static_cast<int>(CoStatus::CO_NOT_FINISH)));
        });
    }
}

static inline void SaveQueueTrafficRecord()
{
    FFRT_BBOX_LOG("<<<=== Queue Traffic Record ===>>>");

    std::string trafficInfo = TrafficRecord::DumpTrafficInfo(false);
    std::stringstream ss;
    ss << trafficInfo;
    FFRT_BBOX_LOG("%s", ss.str().c_str());
    return;
}

static std::atomic_uint g_bbox_tid_is_dealing {0};
static std::atomic_uint g_bbox_called_times {0};

void BboxFreeze()
{
    while (g_bbox_tid_is_dealing.load() != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_PID_SLEEP_MS));
    }
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
        FFRT_BBOX_LOG("debug log: tid %d, task id %lu, qos %d, name %s", gettid(), t->gid, t->qos_(), t->label.c_str());
    }
    SaveKeyStatus();
    FFRT_BBOX_LOG("<<<=== ffrt debug log finish ===>>>");
}

/**
 * @brief BBOX信息记录，包括task、queue、worker相关信息
 *
 * @param void
 * @return void
 * @约束：
 *  1、FFRT模块收到信号，记录BBOX信息，支持信号如下：
 *     SIGABRT、SIGBUS、SIGFPE、SIGILL、SIGSTKFLT、SIGSTOP、SIGSYS、SIGTRAP
 * @规格：
 *  1.调用时机：FFRT模块收到信号时
 *  2.影响：1）FFRT功能不可用，FFRT任务不再执行
 *          2）影响范围仅影响FFRT任务运行，不能造成处理过程中的空指针等异常，如ffrt处理过程造成进行Crash
 */
void SaveTheBbox()
{
    FFRT_BBOX_LOG("<<<=== ffrt black box(BBOX) start ===>>>");
    SaveCurrent();
#if (FFRT_TRACE_RECORD_LEVEL >= FFRT_TRACE_RECORD_LEVEL_2)
    SaveTaskCounter();
#endif
    SaveWorkerStatus();
    SaveKeyStatus();
    SaveNormalTaskStatus();
    SaveQueueTaskStatus();
    SaveQueueTrafficRecord();
    FFRT_BBOX_LOG("<<<=== ffrt black box(BBOX) finish ===>>>");
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

static void HandleChildProcess()
{
    BBoxDeInit();
    pid_t childPid = (pid_t)syscall(SYS_clone, SIGCHLD, 0);
    if (childPid == 0) {
        // init is false to avoid deadlock occurs in the signal handling function due to memory allocation calls.
        auto ctx = ExecuteCtx::Cur(false);
        g_cur_task = ctx != nullptr ? ctx->task : nullptr;
        g_bbox_tid_is_dealing.store(gettid());
        SaveTheBbox();
        g_bbox_tid_is_dealing.store(0);
#ifdef OHOS_STANDARD_SYSTEM
        FaultLoggerFdManager::CloseFd();
#endif
        _exit(0);
    } else if (childPid > 0) {
        pid_t wpid;
        unsigned int remainingRetries = WAIT_PID_MAX_RETRIES;
        while ((wpid = waitpid(childPid, nullptr, WNOHANG)) == 0 && remainingRetries-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_PID_SLEEP_MS));
        }
        if (wpid == 0) {
            (void)kill(childPid, SIGKILL);
        }
    }
}

static void SignalHandler(int signo, siginfo_t* info, void* context __attribute__((unused)))
{
    if (FFRTIsWork() && g_bbox_called_times.fetch_add(1) == 0) { // only save once
        g_cur_pid = static_cast<unsigned int>(getpid());
        g_cur_tid = static_cast<unsigned int>(gettid());
        g_cur_signame = GetSigName(info);
        if (getKeyStatus != nullptr) {
            getKeyStatus();
        }
#ifdef OHOS_STANDARD_SYSTEM
        FaultLoggerFdManager::InitFaultLoggerFd();
#endif
        pid_t childPid = static_cast<pid_t>(syscall(SYS_clone, SIGCHLD, 0));
        if (childPid == 0) {
            HandleChildProcess();
#ifdef OHOS_STANDARD_SYSTEM
            FaultLoggerFdManager::CloseFd();
#endif
            _exit(0);
        } else if (childPid > 0) {
            g_bbox_tid_is_dealing.store(g_cur_tid);
            waitpid(childPid, nullptr, 0);
            g_bbox_tid_is_dealing.store(0);
        }
#ifdef OHOS_STANDARD_SYSTEM
        FaultLoggerFdManager::CloseFd();
#endif
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

std::string GetDumpPreface(void)
{
    std::ostringstream ss;
    ss << "|-> Launcher proc ffrt, now:" << FormatDateToString(FFRTTraceRecord::TimeStamp()) << " pid:" << GetPid()
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
    if (task->fromTid) {
        oss << " fromTid " << task->fromTid;
    }
    if (task->createTime) {
        oss << " createTime " << FormatDateToString(task->createTime);
    }
    if (task->executeTime) {
        oss << " executeTime " << FormatDateToString(task->executeTime);
    }
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

void DumpThreadTaskInfo(WorkerThread* thread, int qos, std::ostringstream& ss)
{
    TaskBase* t = thread->curTask;
    pid_t tid = thread->Id();
    if (t == nullptr) {
        ss << "        qos " << qos << ": worker tid " << tid << " is running nothing" << std::endl;
        return;
    }

    switch (thread->curTaskType_) {
        case ffrt_normal_task: {
            TaskFactory<CPUEUTask>::LockMem();
            auto cpuTask = reinterpret_cast<CPUEUTask*>(t);
            if ((!TaskFactory<CPUEUTask>::HasBeenFreed(cpuTask)) && (cpuTask->state != TaskState::EXITED)) {
                ss << "        qos " << qos << ": worker tid " << tid << " normal task is running, task id "
                   << t->gid << " name " << t->GetLabel().c_str();
                AppendTaskInfo(ss, t);
            }
            TaskFactory<CPUEUTask>::UnlockMem();
            ss << std::endl;
            return;
        }
        case ffrt_queue_task: {
            {
                TaskFactory<QueueTask>::LockMem();
                auto queueTask = reinterpret_cast<QueueTask*>(t);
                if ((!SimpleAllocator<QueueTask>::HasBeenFreed(queueTask)) && (!queueTask->GetFinishStatus())) {
                    ss << "        qos " << qos << ": worker tid " << tid << " queue task is running, task id "
                       << t->gid << " name " << t->GetLabel().c_str();
                    AppendTaskInfo(ss, t);
                }
                TaskFactory<QueueTask>::UnlockMem();
            }
            ss << std::endl;
            return;
        }
        case ffrt_io_task: {
            ss << "        qos " << qos << ": worker tid " << tid << " io task is running" << std::endl;
            return;
        }
        case ffrt_invalid_task: {
            return;
        }
        default: {
            ss << "        qos " << qos << ": worker tid " << tid << " uv task is running" << std::endl;
            return;
        }
    }
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
            tidArr.push_back(thread.first->Id());
            DumpThreadTaskInfo(thread.first, i, ss);
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
    TaskFactory<CPUEUTask>::LockMem();
    auto unfree = TaskFactory<CPUEUTask>::GetUnfreedMem();
    auto apply = [&](const char* tag, const std::function<bool(CPUEUTask*)>& filter) {
        std::vector<CPUEUTask*> tmp;
        for (auto task : unfree) {
            auto t = reinterpret_cast<CPUEUTask*>(task);
            auto f = reinterpret_cast<ffrt_function_header_t*>(t->func_storage);
            if (((f->reserve[0] & MASK_FOR_HCS_TASK) != MASK_FOR_HCS_TASK) && filter(t)) {
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
                    << t->qos_() << ",name " << t->label.c_str();
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
    TaskFactory<CPUEUTask>::UnlockMem();

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

std::string SaveQueueTrafficRecordInfo()
{
    std::string ffrtStackInfo;
    std::ostringstream ss;
    ss << "<<<=== Queue Traffic Record ===>>>" << std::endl;
    ffrtStackInfo += ss.str();
    std::string trafficInfo = TrafficRecord::DumpTrafficInfo();
    ffrtStackInfo += trafficInfo;
    return ffrtStackInfo;
}
#endif
#endif /* FFRT_BBOX_ENABLE */
