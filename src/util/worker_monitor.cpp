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

#include "util/worker_monitor.h"
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#ifdef FFRT_OH_TRACE_ENABLE
#include "backtrace_local.h"
#endif

#include "dfx/sysevent/sysevent.h"
#include "eu/execute_unit.h"
#include "eu/execute_unit.h"
#include "eu/co_routine_factory.h"
#include "internal_inc/osal.h"
#include "sched/scheduler.h"
#include "util/ffrt_facade.h"
#include "util/white_list.h"
#include "dfx/bbox/bbox.h"
#include "tm/task_factory.h"

namespace {
constexpr int HISYSEVENT_TIMEOUT_SEC = 60;
constexpr int MONITOR_SAMPLING_CYCLE_US = 500 * 1000;
constexpr unsigned int RECORD_WORKER_STATUS_INFO_FREQ = 5;
constexpr int SAMPLING_TIMES_PER_SEC = 1000 * 1000 / MONITOR_SAMPLING_CYCLE_US;
constexpr uint64_t TIMEOUT_MEMSHRINK_CYCLE_US = 60 * 1000 * 1000;
constexpr int RECORD_IPC_INFO_TIME_THRESHOLD = 600;
constexpr int BACKTRACE_TASK_QOS = 7;
constexpr char IPC_STACK_NAME[] = "libipc_common";
constexpr char TRANSACTION_PATH[] = "/proc/transaction_proc";
const std::vector<int> TIMEOUT_RECORD_CYCLE_LIST = { 1, 5, 10, 30, 60, 10 * 60, 30 * 60 };
constexpr uint32_t US_PER_MS = 1000;
constexpr uint64_t MIN_TIMEOUT_THRESHOLD_US = 1000 * US_PER_MS; // 1s
constexpr uint64_t ALLOW_ACC_ERROR_US = 10 * US_PER_MS; // 10ms
constexpr uint32_t INITIAL_RECORD_LIMIT = 16;
constexpr uint32_t MAX_RECORD_LIMIT = 64;
constexpr int FIRST_THRESHOLD = 50;
constexpr int SECOND_THRESHOLD = 100;
constexpr size_t MAX_FRAME_NUMS = 8;
}

namespace ffrt {
WorkerMonitor::WorkerMonitor()
{
    recycleResourceWaitEntry_.cb = ([this]([[maybe_unused]] WaitEntry* we) {
        if (SetExitFlagIfNoWorkers(recycleResourceExit_)) {
            CoRoutineReleaseMem();
            WorkerStatus();
            return;
        }

        CoRoutineReleaseMem();
        if (samplingTaskCount_++ % RECORD_WORKER_STATUS_INFO_FREQ == 0) {
            WorkerStatus();
        }
        SubmitRecycleResource();
    });
    if (WhiteList::GetInstance().IsEnabled("worker_monitor", false)) {
        FFRT_SYSEVENT_LOGW("Skip worker monitor.");
        skipSampling_ = true;
        return;
    }
    uint64_t timeout = ffrt_task_timeout_get_threshold() * US_PER_MS;
    timeoutUs_ = timeout;
    if (timeout < MIN_TIMEOUT_THRESHOLD_US) {
        FFRT_LOGE("invalid watchdog timeout [%llu] us, using 1s instead", timeout);
        timeoutUs_ = MIN_TIMEOUT_THRESHOLD_US;
    }

    watchdogWaitEntry_.cb = ([this](WaitEntry* we) {
        (void)we;
        CheckWorkerStatus();
        FFRTFacade::GetPPInstance().MonitTimeOut();
    });
    tskMonitorWaitEntry_.cb = ([this](WaitEntry* we) { CheckTaskStatus(); });
}

WorkerMonitor::~WorkerMonitor()
{
    FFRT_LOGW("WorkerMonitor destruction enter");
    std::lock_guard lock(mutex_);
    skipSampling_ = true;
}

WorkerMonitor& WorkerMonitor::GetInstance()
{
    static WorkerMonitor instance;
    return instance;
}

void WorkerMonitor::SubmitTask()
{
    std::lock_guard submitTaskLock(submitTaskMutex_);
    if (!skipSampling_) {
        if (samplingTaskExit_) {
            SubmitSamplingTask();
            samplingTaskExit_ = false;
        }
        if (taskMonitorExit_) {
            SubmitTaskMonitor(timeoutUs_);
            taskMonitorExit_ = false;
        }
    }
    if (recycleResourceExit_) {
        SubmitRecycleResource();
        recycleResourceExit_ = false;
    }
}

void WorkerMonitor::SubmitSamplingTask()
{
    watchdogWaitEntry_.tp = std::chrono::steady_clock::now() + std::chrono::microseconds(MONITOR_SAMPLING_CYCLE_US);
    if (!DelayedWakeup(watchdogWaitEntry_.tp, &watchdogWaitEntry_, watchdogWaitEntry_.cb, true)) {
        FFRT_LOGW("Set delayed worker failed.");
    }
}

void WorkerMonitor::SubmitTaskMonitor(uint64_t nextTimeoutUs)
{
    tskMonitorWaitEntry_.tp = std::chrono::steady_clock::now() + std::chrono::microseconds(nextTimeoutUs);
    if (!DelayedWakeup(tskMonitorWaitEntry_.tp, &tskMonitorWaitEntry_, tskMonitorWaitEntry_.cb, true)) {
        FFRT_LOGW("Set delayed worker failed.");
    }
}

void WorkerMonitor::SubmitRecycleResource()
{
    recycleResourceWaitEntry_.tp = std::chrono::steady_clock::now() +
        std::chrono::microseconds(TIMEOUT_MEMSHRINK_CYCLE_US);
    if (!DelayedWakeup(recycleResourceWaitEntry_.tp, &recycleResourceWaitEntry_, recycleResourceWaitEntry_.cb, true)) {
        FFRT_LOGW("Set delayed worker failed.");
    }
}

void WorkerMonitor::CheckWorkerStatus()
{
    if (SetExitFlagIfNoWorkers(samplingTaskExit_)) {
        std::lock_guard lock(mutex_);
        workerStatus_ = std::unordered_map<CPUWorker*, TaskTimeoutInfo>();
        return;
    }

    std::vector<TimeoutFunctionInfo> timeoutFunctions;
    for (int i = 0; i < QoS::MaxNum(); i++) {
        CPUWorkerGroup& workerGroup = FFRTFacade::GetEUInstance().GetWorkerGroup(i);
        std::shared_lock<std::shared_mutex> lck(workerGroup.tgMutex);
        int executingNum = 0;
        int sleepingNum  = 0;
        {
            std::lock_guard lg(workerGroup.lock);
            executingNum =  workerGroup.executingNum;
            sleepingNum  = workerGroup.sleepingNum;
        }
        CoWorkerInfo coWorkerInfo(i, workerGroup.threads.size(), executingNum, sleepingNum);
        for (auto& thread : workerGroup.threads) {
            CPUWorker* worker = thread.first;
            if (!worker->Monitor()) {
                continue;
            }

            TaskBase* workerTask = worker->curTask.load(std::memory_order_relaxed);
            if (workerTask == nullptr) {
                workerStatus_.erase(worker);
                continue;
            }

            RecordTimeoutFunctionInfo(coWorkerInfo, worker, workerTask, timeoutFunctions);
        }
    }

    if (timeoutFunctions.size() > 0) {
        FFRTFacade::GetDWInstance().SubmitAsyncTask([this, timeoutFunctions] {
            for (const auto& timeoutFunction : timeoutFunctions) {
                RecordSymbolAndBacktrace(timeoutFunction);
            }
        });
    }

    SubmitSamplingTask();
}

void WorkerMonitor::CheckTaskStatus()
{
    std::vector<CPUEUTask*> activeTask;
    auto unfree = TaskFactory<CPUEUTask>::GetUnfreedTasksFiltered();
    for (auto task : unfree) {
        auto t = reinterpret_cast<CPUEUTask*>(task);
        if (t->type == ffrt_normal_task && t->aliveStatus.load(std::memory_order_relaxed) == AliveStatus::INITED) {
            activeTask.emplace_back(t);
        }
    }

    // no active worker, no active normal task, no need to monitor
    if (SetExitFlagIfNoWorkers(taskMonitorExit_) && activeTask.empty()) {
        for (auto& task : unfree) {
            reinterpret_cast<CPUEUTask*>(task)->DecDeleteRef();
        }
        std::lock_guard lock(mutex_);
        taskTimeoutInfo_.clear();
        return;
    }

    uint64_t now = TimeStampCntvct();
    uint64_t minStart = now - ((timeoutUs_ - ALLOW_ACC_ERROR_US));
    uint64_t curMinTimeStamp = UINT64_MAX;

    for (auto task : activeTask) {
        uint64_t curTimeStamp = CalculateTaskTimeout(task, minStart);
        curMinTimeStamp = curTimeStamp < curMinTimeStamp ? curTimeStamp : curMinTimeStamp;
    }
    for (auto& task : unfree) {
        reinterpret_cast<CPUEUTask*>(task)->DecDeleteRef();
    }

    // 下次检查时间为所有当前任务中的最小状态时间
    uint64_t nextTimeout = (curMinTimeStamp == UINT64_MAX) ? timeoutUs_ :
        std::min(timeoutUs_, timeoutUs_ - (now - curMinTimeStamp));

    SubmitTaskMonitor(nextTimeout);
}

uint64_t WorkerMonitor::CalculateTaskTimeout(CPUEUTask* task, uint64_t timeoutThreshold)
{
    // 主动延时的任务不检测
    if (!task->monitorTimeout_ || isDelayingTask(task) ||
        (task->delayTime > 0 && task->curStatus == TaskStatus::SUBMITTED)) {
        return UINT64_MAX;
    }

    uint64_t curTaskTime = task->statusTime.load(std::memory_order_relaxed);
    uint64_t timeoutCount = task->timeoutTask.timeoutCnt;

    if (curTaskTime + timeoutCount * timeoutUs_ < timeoutThreshold) {
        RecordTimeoutTask(task);
        return UINT64_MAX;
    }
    return curTaskTime;
}

bool WorkerMonitor::ControlTimeoutFreq(CPUEUTask* task)
{
    uint64_t timoutCnt = task->timeoutTask.timeoutCnt;
    return (timoutCnt < SECOND_THRESHOLD) ? ((timoutCnt % FIRST_THRESHOLD) == 1) :
        ((timoutCnt % SECOND_THRESHOLD) == 1);
}

/**
 * @brief 记录任务超时信息并生成日志，用于监控任务执行状态和超时情况。
 *
 * 日志字段格式示例：label|gid|Qos|delayTime|curTaskStatus|curTime|preTaskStatus|timeoutRatio|timeoutCount
 */
void WorkerMonitor::RecordTimeoutTask(CPUEUTask* task)
{
    TaskStatus curTaskStatus = task->curStatus;
    uint64_t curTaskTime = task->statusTime.load(std::memory_order_relaxed);
    TaskStatus preTaskStatus = task->preStatus.load(std::memory_order_relaxed);

    TimeoutTask& timeoutTskInfo = task->timeoutTask;
    if (task->gid == timeoutTskInfo.taskGid && curTaskStatus == timeoutTskInfo.taskStatus) {
        timeoutTskInfo.timeoutCnt += 1;
    } else {
        timeoutTskInfo.timeoutCnt = 1;
        timeoutTskInfo.taskStatus = curTaskStatus;
        timeoutTskInfo.taskGid = task->gid;
    }

    if (!ControlTimeoutFreq(task)) {
        return;
    }

    std::stringstream ss;
    uint64_t time = TimeStampCntvct();

    ss << task->label.c_str() << "|" << task->gid << "|" << task->GetQos() <<
        "|" << task->delayTime << "|" << StatusToString(curTaskStatus) <<
        "|" << FormatDateString4SteadyClock(curTaskTime, TimeUnitT::MILLISECOND, "%H:%M:%S", false);

    {
        std::lock_guard lock(mutex_);
        if (taskTimeoutInfo_.size() > MAX_RECORD_LIMIT) {
            taskTimeoutInfo_.pop_front();
        }
        taskTimeoutInfo_.emplace_back(time, ss.str());
    }

    ss << "|" << StatusToString(preTaskStatus) << "|" <<
        timeoutUs_ / MIN_TIMEOUT_THRESHOLD_US << "|" << timeoutTskInfo.timeoutCnt;
    FFRT_LOGW("%s", ss.str().c_str());
    return;
}

std::string WorkerMonitor::DumpTimeoutInfo()
{
    std::lock_guard lock(mutex_);
    std::stringstream ss;
    if (taskTimeoutInfo_.size() != 0) {
        for (auto it = taskTimeoutInfo_.rbegin(); it != taskTimeoutInfo_.rend(); ++it) {
            auto& record = *it;
            ss << "{" << FormatDateString4SteadyClock(record.first) << ", " << record.second << "} \n";
        }
    } else {
        ss << "Timeout info Empty";
    }
    return ss.str();
}

void WorkerMonitor::RecordTimeoutFunctionInfo(const CoWorkerInfo& coWorkerInfo, CPUWorker* worker,
    TaskBase* workerTask, std::vector<TimeoutFunctionInfo>& timeoutFunctions)
{
    auto workerIter = workerStatus_.find(worker);
    if (workerIter == workerStatus_.end()) {
        workerStatus_[worker] = TaskTimeoutInfo(workerTask);
        return;
    }

    TaskTimeoutInfo& taskInfo = workerIter->second;
    if (taskInfo.task_ == workerTask) {
        if (++taskInfo.sampledTimes_ < SAMPLING_TIMES_PER_SEC) {
            return;
        }

        taskInfo.sampledTimes_ = 0;
        if (++taskInfo.executionTime_ % TIMEOUT_RECORD_CYCLE_LIST[taskInfo.recordLevel_] == 0) {
            WorkerInfo workerInfo(worker->Id(), worker->curTaskGid_,
                worker->curTaskType_.load(std::memory_order_relaxed), worker->curTaskLabel_);
            timeoutFunctions.emplace_back(coWorkerInfo, workerInfo, taskInfo.executionTime_);
            if (taskInfo.recordLevel_ < static_cast<int>(TIMEOUT_RECORD_CYCLE_LIST.size()) - 1) {
                taskInfo.recordLevel_++;
            }
        }

        return;
    }

    if (taskInfo.executionTime_ > 0) {
        FFRT_LOGI("Tid[%d] function is executed, which occupies worker for [%d]s.",
            worker->Id(), taskInfo.executionTime_);
    }
    workerIter->second = TaskTimeoutInfo(workerTask);
}

void WorkerMonitor::RecordSymbolAndBacktrace(const TimeoutFunctionInfo& timeoutFunction)
{
    std::stringstream ss;
    std::string processNameStr = std::string(GetCurrentProcessName());
    ss << "Process:" << processNameStr << ",Tid:" << timeoutFunction.workerInfo_.tid_ <<
        ",Qos:" << timeoutFunction.coWorkerInfo_.qosLevel_ << ",CWorker:" <<
        timeoutFunction.coWorkerInfo_.coWorkerCount_ << ",EWorker:" <<
        timeoutFunction.coWorkerInfo_.executionNum_ << ",SWorker:" <<
        timeoutFunction.coWorkerInfo_.sleepingWorkerNum_ << ",TaskType:" <<
        timeoutFunction.workerInfo_.workerTaskType_ << ",";

#ifdef WORKER_CACHE_TASKNAMEID
    if (timeoutFunction.workerInfo_.workerTaskType_ == ffrt_normal_task ||
        timeoutFunction.workerInfo_.workerTaskType_ == ffrt_queue_task) {
        ss << "TaskName:" << timeoutFunction.workerInfo_.label_ <<
            ",TaskId:" << timeoutFunction.workerInfo_.gid_ << ",";
    }
#endif

    ss << "timeout:" << timeoutFunction.executionTime_ << "s";
    FFRT_LOGW("%s", ss.str().c_str());

#ifdef FFRT_OH_TRACE_ENABLE
    std::string dumpInfo;
    // Set max frame nums to 8 for the first timeout level for shorter output
    int maxFrameNums = (timeoutFunction.executionTime_ == TIMEOUT_RECORD_CYCLE_LIST[0]) ? MAX_FRAME_NUMS :
        OHOS::HiviewDFX::DEFAULT_MAX_FRAME_NUM;
    if (OHOS::HiviewDFX::GetBacktraceStringByTid(dumpInfo, timeoutFunction.workerInfo_.tid_, 0, false, maxFrameNums)) {
        FFRT_LOGW("%s", dumpInfo.c_str());
        if (timeoutFunction.executionTime_ >= RECORD_IPC_INFO_TIME_THRESHOLD) {
            RecordIpcInfo(dumpInfo, timeoutFunction.workerInfo_.tid_);
        }
    }
#endif
#ifdef FFRT_SEND_EVENT
    if (timeoutFunction.executionTime_ == HISYSEVENT_TIMEOUT_SEC) {
        std::string senarioName = "Task_Sch_Timeout";
        TaskTimeoutReport(ss, processNameStr, senarioName);
    }
#endif
}

void WorkerMonitor::RecordIpcInfo(const std::string& dumpInfo, int tid)
{
    if (dumpInfo.find(IPC_STACK_NAME) == std::string::npos) {
        return;
    }

    std::ifstream transactionFile(TRANSACTION_PATH);
    FFRT_COND_DO_ERR(!transactionFile.is_open(), return, "open transaction_proc failed");

    FFRT_LOGW("transaction_proc:");
    std::string line;
    std::string regexStr = ".*" + std::to_string(tid) + ".*to.*code.*";
    while (getline(transactionFile, line)) {
        if (std::regex_match(line, std::regex(regexStr))) {
            FFRT_LOGW("%s", line.c_str());
        }
    }

    transactionFile.close();
}

void WorkerMonitor::RecordKeyInfo(const std::string& dumpInfo)
{
    if (dumpInfo.find(IPC_STACK_NAME) == std::string::npos || dumpInfo.find("libpower") == std::string::npos) {
        return;
    }

#ifdef FFRT_CO_BACKTRACE_OH_ENABLE
    std::string keyInfo = SaveKeyInfo();
    FFRT_LOGW("%s", keyInfo.c_str());
#endif
}

void WorkerMonitor::ProcessWorkerInfo(std::ostringstream& oss, bool& firstQos, int qos, unsigned int cnt,
    const std::deque<pid_t>& tids)
{
    if (cnt == 0) {
        return;
    }

    if (!firstQos) {
        oss << " ";
    }
    firstQos = false;

    oss << qos << "|" << cnt << "|";
    bool firstTid = true;
    for (const auto& tid : tids) {
        if (!firstTid) {
            oss << ",";
        }
        firstTid = false;
        oss << tid;
    }
}

void WorkerMonitor::WorkerStatus()
{
    std::ostringstream startedOss;
    std::ostringstream exitedOss;
    bool startedFirstQos = true;
    bool exitedFirstQos = true;

    for (int qos = 0; qos < QoS::MaxNum(); qos++) {
        auto workerStatusInfo = FFRTFacade::GetEUInstance().GetWorkerStatusInfoAndReset(qos);
        ProcessWorkerInfo(startedOss, startedFirstQos, qos, workerStatusInfo.startedCnt, workerStatusInfo.startedTids);
        ProcessWorkerInfo(exitedOss, exitedFirstQos, qos, workerStatusInfo.exitedCnt, workerStatusInfo.exitedTids);
    }

    if (!startedOss.str().empty()) {
        FFRT_LOGW("%s", startedOss.str().c_str());
    }
    if (!exitedOss.str().empty()) {
        FFRT_LOGW("%s", exitedOss.str().c_str());
    }
}

bool WorkerMonitor::SetExitFlagIfNoWorkers(bool& exitFlag)
{
    bool noWorkerThreads = true;
    {
        std::lock_guard submitTaskLock(submitTaskMutex_);
        for (int i = 0; i < QoS::MaxNum(); i++) {
            CPUWorkerGroup& workerGroup = FFRTFacade::GetEUInstance().GetWorkerGroup(i);
            std::shared_lock<std::shared_mutex> lck(workerGroup.tgMutex);
            if (!workerGroup.threads.empty()) {
                noWorkerThreads = false;
                break;
            }
        }
        if (noWorkerThreads) {
            exitFlag = true;
        }
    }
    return noWorkerThreads;
}
}
