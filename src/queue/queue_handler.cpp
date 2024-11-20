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
#include "queue_handler.h"
#include <sys/syscall.h>
#include <sstream>
#include "dfx/log/ffrt_log_api.h"
#include "dfx/trace_record/ffrt_trace_record.h"
#include "util/event_handler_adapter.h"
#include "util/ffrt_facade.h"
#include "util/slab.h"
#include "tm/queue_task.h"
#include "concurrent_queue.h"
#include "eventhandler_adapter_queue.h"
#include "sched/scheduler.h"

namespace {
constexpr int PROCESS_NAME_BUFFER_LENGTH = 1024;
constexpr uint32_t STRING_SIZE_MAX = 128;
constexpr uint32_t TASK_DONE_WAIT_UNIT = 10;
constexpr uint64_t SCHED_TIME_ACC_ERROR_US = 5000; // 5ms
constexpr uint32_t CONGESTION_CNT = 5;
constexpr uint32_t CONGESTION_TIMEOUT_US = 300000000; // 5min
}

namespace ffrt {
QueueHandler::QueueHandler(const char* name, const ffrt_queue_attr_t* attr, const int type)
{
    // parse queue attribute
    if (attr) {
        qos_ = (ffrt_queue_attr_get_qos(attr) >= ffrt_qos_background) ? ffrt_queue_attr_get_qos(attr) : qos_;
        timeout_ = ffrt_queue_attr_get_timeout(attr);
        timeoutCb_ = ffrt_queue_attr_get_callback(attr);
    }

    // callback reference counting is to ensure life cycle
    if (timeout_ > 0 && timeoutCb_ != nullptr) {
        QueueTask* cbTask = GetQueueTaskByFuncStorageOffset(timeoutCb_);
        cbTask->IncDeleteRef();
    }

    queue_ = CreateQueue(type, attr);
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "[queueId=%u] constructed failed", GetQueueId());

    if (name != nullptr && std::string(name).size() <= STRING_SIZE_MAX) {
        name_ = "sq_" + std::string(name) + "_" + std::to_string(GetQueueId());
    } else {
        name_ += "sq_unnamed_" + std::to_string(GetQueueId());
        FFRT_LOGW("failed to set [queueId=%u] name due to invalid name or length.", GetQueueId());
    }

    FFRTFacade::GetQMInstance().RegisterQueueId(GetQueueId(), this);
    FFRT_LOGI("construct %s succ, qos[%d]", name_.c_str(), qos_);
}

QueueHandler::~QueueHandler()
{
    FFRT_LOGI("destruct %s enter", name_.c_str());
    // clear tasks in queue
    CancelAndWait();
    FFRTFacade::GetQMInstance().ResetQueueStruct(GetQueueId());

    // release callback resource
    if (timeout_ > 0) {
        // wait for all delayedWorker to complete.
        while (delayedCbCnt_.load() > 0) {
            this_task::sleep_for(std::chrono::microseconds(timeout_));
        }

        if (timeoutCb_ != nullptr) {
            QueueTask* cbTask = GetQueueTaskByFuncStorageOffset(timeoutCb_);
            cbTask->DecDeleteRef();
        }
    }

    if (we_ != nullptr) {
        DelayedRemove(we_->tp, we_);
        SimpleAllocator<WaitUntilEntry>::FreeMem(we_);
    }
    FFRT_LOGI("destruct %s leave", name_.c_str());
}

bool QueueHandler::SetLoop(Loop* loop)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return false, "[queueId=%u] constructed failed", GetQueueId());
    if (queue_->GetQueueType() == ffrt_queue_eventhandler_interactive) {
        return true;
    }
    FFRT_COND_DO_ERR((queue_->GetQueueType() != ffrt_queue_concurrent),
        return false, "[queueId=%u] type invalid", GetQueueId());
    return reinterpret_cast<ConcurrentQueue*>(queue_.get())->SetLoop(loop);
}

bool QueueHandler::ClearLoop()
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return false, "[queueId=%u] constructed failed", GetQueueId());
    FFRT_COND_DO_ERR((queue_->GetQueueType() != ffrt_queue_concurrent),
        return false, "[queueId=%u] type invalid", GetQueueId());
    return reinterpret_cast<ConcurrentQueue*>(queue_.get())->ClearLoop();
}

QueueTask* QueueHandler::PickUpTask()
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return nullptr, "[queueId=%u] constructed failed", GetQueueId());
    return queue_->Pull();
}

void QueueHandler::Submit(QueueTask* task)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "cannot submit, [queueId=%u] constructed failed", GetQueueId());
    FFRT_COND_DO_ERR((task == nullptr), return, "input invalid, serial task is nullptr");

    // if qos not specified, qos of the queue is inherited by task
    if (task->GetQos() == qos_inherit || task->GetQos() == qos_default) {
        task->SetQos(qos_);
    }

    uint64_t gid = task->gid;
    FFRT_SERIAL_QUEUE_TASK_SUBMIT_MARKER(GetQueueId(), gid);
    FFRTTraceRecord::TaskSubmit(&(task->createTime), &(task->fromTid));
#if (FFRT_TRACE_RECORD_LEVEL < FFRT_TRACE_RECORD_LEVEL_1)
    if (queue_->GetQueueType() == ffrt_queue_eventhandler_adapter) {
        task->fromTid = ExecuteCtx::Cur()->tid;
    }
#endif

    // work after that schedule timeout is set for queue
    if (task->GetSchedTimeout() > 0) {
        AddSchedDeadline(task);
    }
    if (we_ != nullptr) {
        CheckOverload();
    }

    int ret = queue_->Push(task);
    if (ret == SUCC) {
        FFRT_LOGD("submit task[%lu] into %s", gid, name_.c_str());
        return;
    }
    if (ret == FAILED) {
        return;
    }

    if (!isUsed_.load()) {
        isUsed_.store(true);
    }

    // activate queue
    if (task->GetDelay() == 0) {
        FFRT_LOGD("task [%llu] activate %s", gid, name_.c_str());
        TransferTask(task);
    } else {
        FFRT_LOGD("task [%llu] with delay [%llu] activate %s", gid, task->GetDelay(), name_.c_str());
        if (ret == INACTIVE) {
            queue_->Push(task);
        }
        TransferInitTask();
    }
}

void QueueHandler::Cancel()
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "cannot cancel, [queueId=%u] constructed failed", GetQueueId());
    queue_->Remove();
}

void QueueHandler::CancelAndWait()
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "cannot cancelAndWait, [queueId=%u] constructed failed",
        GetQueueId());
    queue_->Stop();
    while (FFRTFacade::GetQMInstance().QueryQueueStatus(GetQueueId()) || queue_->GetActiveStatus()) {
        std::this_thread::sleep_for(std::chrono::microseconds(TASK_DONE_WAIT_UNIT));
    }
}

int QueueHandler::Cancel(const char* name)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return INACTIVE,
         "cannot cancel, [queueId=%u] constructed failed", GetQueueId());
    int ret = queue_->Remove(name);
    if (ret != SUCC) {
        FFRT_LOGD("cancel task %s failed, task may have been executed", name);
    }

    return ret;
}

int QueueHandler::Cancel(QueueTask* task)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return INACTIVE,
         "cannot cancel, [queueId=%u] constructed failed", GetQueueId());
    FFRT_COND_DO_ERR((task == nullptr), return INACTIVE, "input invalid, serial task is nullptr");

    if (task->GetSchedTimeout() > 0) {
        RemoveSchedDeadline(task);
    }

    int ret = queue_->Remove(task);
    if (ret == SUCC) {
        FFRT_LOGD("cancel task[%llu] %s succ", task->gid, task->label.c_str());
        task->Notify();
        task->Destroy();
    } else {
        FFRT_LOGD("cancel task[%llu] %s failed, task may have been executed", task->gid, task->label.c_str());
    }
    return ret;
}

void QueueHandler::Dispatch(QueueTask* inTask)
{
    QueueTask* nextTask = nullptr;
    for (QueueTask* task = inTask; task != nullptr; task = nextTask) {
        // dfx watchdog
        SetTimeoutMonitor(task);
        FFRTFacade::GetQMInstance().UpdateQueueInfo(GetQueueId(), task->gid);
        execTaskId_.store(task->gid);

        // run user task
        FFRT_LOGD("run task [gid=%llu], queueId=%u", task->gid, GetQueueId());
        auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
        FFRT_SERIAL_QUEUE_TASK_EXECUTE_MARKER(task->gid);
        FFRTTraceRecord::TaskExecute(&(task->executeTime));
        if (task->GetSchedTimeout() > 0) {
            RemoveSchedDeadline(task);
        }

        uint64_t triggerTime{0};
        if (queue_->GetQueueType() == ffrt_queue_eventhandler_adapter) {
            triggerTime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        }

        f->exec(f);
        FFRTTraceRecord::TaskDone<ffrt_queue_task>(task->GetQos(), task);
        if (queue_->GetQueueType() == ffrt_queue_eventhandler_adapter) {
            uint64_t completeTime = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
            reinterpret_cast<EventHandlerAdapterQueue*>(queue_.get())->PushHistoryTask(task, triggerTime, completeTime);
        }

        f->destroy(f);
        task->Notify();

        // run task batch
        nextTask = task->GetNextTask();
        if (nextTask == nullptr) {
            FFRTFacade::GetQMInstance().ResetQueueInfo(GetQueueId());
            execTaskId_.store(0);
            if (!queue_->IsOnLoop()) {
                Deliver();
            }
        }
        task->DecDeleteRef();
    }
}

void QueueHandler::Deliver()
{
    QueueTask* task = queue_->Pull();
    if (task != nullptr) {
        TransferTask(task);
    }
}

void QueueHandler::TransferTask(QueueTask* task)
{
    auto entry = &task->fq_we;
    if (queue_->GetQueueType() == ffrt_queue_eventhandler_adapter) {
        reinterpret_cast<EventHandlerAdapterQueue*>(queue_.get())->SetCurrentRunningTask(task);
    }
    FFRTScheduler* sch = FFRTFacade::GetSchedInstance();
    FFRT_READY_MARKER(task->gid); // ffrt queue task ready to enque
    if (!sch->InsertNode(&entry->node, task->GetQos())) {
        FFRT_LOGE("failed to insert task [%llu] into %s", task->gid, name_.c_str());
        return;
    }
}

void QueueHandler::TransferInitTask()
{
    std::function<void()> initFunc = [] {};
    auto f = create_function_wrapper(initFunc, ffrt_function_kind_queue);
    QueueTask* initTask = GetQueueTaskByFuncStorageOffset(f);
    new (initTask)ffrt::QueueTask(this);
    initTask->SetQos(qos_);
    TransferTask(initTask);
}

void QueueHandler::SetTimeoutMonitor(QueueTask* task)
{
    if (timeout_ <= 0) {
        return;
    }

    task->IncDeleteRef();
    WaitUntilEntry* we = new (SimpleAllocator<WaitUntilEntry>::AllocMem()) WaitUntilEntry();
    // set delayed worker callback
    we->cb = ([this, task](WaitEntry* we) {
        if (!task->GetFinishStatus()) {
            RunTimeOutCallback(task);
        }
        delayedCbCnt_.fetch_sub(1);
        task->DecDeleteRef();
        SimpleAllocator<WaitUntilEntry>::FreeMem(static_cast<WaitUntilEntry*>(we));
    });

    // set delayed worker wakeup time
    std::chrono::microseconds timeout(timeout_);
    auto now = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());
    we->tp = std::chrono::time_point_cast<std::chrono::steady_clock::duration>(now + timeout);

    if (!DelayedWakeup(we->tp, we, we->cb)) {
        task->DecDeleteRef();
        SimpleAllocator<WaitUntilEntry>::FreeMem(we);
        FFRT_LOGW("failed to set watchdog for task gid=%llu in %s with timeout [%llu us] ", task->gid,
            name_.c_str(), timeout_);
        return;
    }

    delayedCbCnt_.fetch_add(1);
    FFRT_LOGD("set watchdog of task gid=%llu of %s succ", task->gid, name_.c_str());
}

void QueueHandler::RunTimeOutCallback(QueueTask* task)
{
    std::stringstream ss;
    static std::once_flag flag;
    static char processName[PROCESS_NAME_BUFFER_LENGTH];
    std::call_once(flag, []() {
        GetProcessName(processName, PROCESS_NAME_BUFFER_LENGTH);
    });
    std::string processNameStr = std::string(processName);
    ss << "[Serial_Queue_Timeout_Callback] process name:[" << processNameStr << "], serial queue:[" <<
        name_ << "], queueId:[" << GetQueueId() << "], serial task gid:[" << task->gid << "], task name:["
        << task->label << "], execution time exceeds[" << timeout_ << "] us";
    FFRT_LOGE("%s", ss.str().c_str());
    if (timeoutCb_ != nullptr) {
        timeoutCb_->exec(timeoutCb_);
    }
}

std::string QueueHandler::GetDfxInfo() const
{
    std::stringstream ss;
    ss << " queue name [" << name_ << "]";
    if (queue_ != nullptr) {
        ss << ", remaining tasks count=" << queue_->GetMapSize();
    }
    return ss.str();
}

bool QueueHandler::IsIdle()
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return false, "[queueId=%u] constructed failed", GetQueueId());
    FFRT_COND_DO_ERR((queue_->GetQueueType() != ffrt_queue_eventhandler_adapter),
        return false, "[queueId=%u] type invalid", GetQueueId());

    return reinterpret_cast<EventHandlerAdapterQueue*>(queue_.get())->IsIdle();
}

void QueueHandler::SetEventHandler(void* eventHandler)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "[queueId=%u] constructed failed", GetQueueId());

    bool typeInvalid = (queue_->GetQueueType() != ffrt_queue_eventhandler_interactive) &&
        (queue_->GetQueueType() != ffrt_queue_eventhandler_adapter);
    FFRT_COND_DO_ERR(typeInvalid, return, "[queueId=%u] type invalid", GetQueueId());

    reinterpret_cast<EventHandlerInteractiveQueue*>(queue_.get())->SetEventHandler(eventHandler);
}

void* QueueHandler::GetEventHandler()
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return nullptr, "[queueId=%u] constructed failed", GetQueueId());
 
    bool typeInvalid = (queue_->GetQueueType() != ffrt_queue_eventhandler_interactive) &&
        (queue_->GetQueueType() != ffrt_queue_eventhandler_adapter);
    FFRT_COND_DO_ERR(typeInvalid, return nullptr, "[queueId=%u] type invalid", GetQueueId());

    return reinterpret_cast<EventHandlerInteractiveQueue*>(queue_.get())->GetEventHandler();
}

int QueueHandler::Dump(const char* tag, char* buf, uint32_t len, bool historyInfo)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return -1, "[queueId=%u] constructed failed", GetQueueId());
    FFRT_COND_DO_ERR((queue_->GetQueueType() != ffrt_queue_eventhandler_adapter),
        return -1, "[queueId=%u] type invalid", GetQueueId());
    return reinterpret_cast<EventHandlerAdapterQueue*>(queue_.get())->Dump(tag, buf, len, historyInfo);
}

int QueueHandler::DumpSize(ffrt_inner_queue_priority_t priority)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return -1, "[queueId=%u] constructed failed", GetQueueId());
    FFRT_COND_DO_ERR((queue_->GetQueueType() != ffrt_queue_eventhandler_adapter),
        return -1, "[queueId=%u] type invalid", GetQueueId());
    return reinterpret_cast<EventHandlerAdapterQueue*>(queue_.get())->DumpSize(priority);
}

void QueueHandler::SendSchedTimer(TimePoint delay)
{
    we_->tp = delay;
    bool result = DelayedWakeup(we_->tp, we_, we_->cb);
    while (!result) {
        FFRT_LOGW("failed to set delayedworker, retry");
        we_->tp = std::chrono::steady_clock::now() + std::chrono::microseconds(SCHED_TIME_ACC_ERROR_US);
        result = DelayedWakeup(we_->tp, we_, we_->cb);
    }
}

void QueueHandler::CheckSchedDeadline()
{
    std::vector<uint64_t> timeoutTaskId;
    // Collecting Timeout Tasks
    {
        std::unique_lock lock(mutex_);
        uint64_t threshold = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count() + SCHED_TIME_ACC_ERROR_US;

        auto it = schedDeadline_.begin();
        uint64_t nextDeadline = UINT64_MAX;
        while (it != schedDeadline_.end()) {
            if (it->second < threshold) {
                timeoutTaskId.push_back(it->first->gid);
                it = schedDeadline_.erase(it);
            } else {
                nextDeadline = std::min(nextDeadline, it->second);
                ++it;
            }
        }

        if (schedDeadline_.empty()) {
            initSchedTimer_ = false;
        } else {
            std::chrono::microseconds timeout(nextDeadline);
            TimePoint tp = std::chrono::time_point_cast<std::chrono::steady_clock::duration>(
                std::chrono::steady_clock::time_point() + timeout);
            FFRT_LOGI("queueId=%u set sched timer", GetQueueId());
            SendSchedTimer(tp);
        }
    }

    // Reporting Timeout Infomation
    if (!timeoutTaskId.empty()) {
        ReportTimeout(timeoutTaskId);
    }
}

void QueueHandler::AddSchedDeadline(QueueTask* task)
{
    // sched timeout only support serial queues, other queue types will be supported based on service requirements.
    if (queue_->GetQueueType() != ffrt_queue_serial) {
        return;
    }

    std::unique_lock lock(mutex_);
    schedDeadline_.insert({task, task->GetSchedTimeout() + task->GetUptime()});

    if (!initSchedTimer_) {
        if (we_ == nullptr) {
            we_ = new (SimpleAllocator<WaitUntilEntry>::AllocMem()) WaitUntilEntry();
            we_->cb = ([this](WaitEntry* we) { CheckSchedDeadline(); });
        }
        std::chrono::microseconds timeout(schedDeadline_[task]);
        TimePoint tp = std::chrono::time_point_cast<std::chrono::steady_clock::duration>(
            std::chrono::steady_clock::time_point() + timeout);
        SendSchedTimer(tp);
        initSchedTimer_ = true;
    }
}

void QueueHandler::RemoveSchedDeadline(QueueTask* task)
{
    std::unique_lock lock(mutex_);
    schedDeadline_.erase(task);
}

void QueueHandler::CheckOverload()
{
    if (queue_->GetMapSize() <= CONGESTION_CNT) {
        return;
    }

    uint64_t expect = queue_->GetHeadUptime();
    uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
    if (now > expect && now - expect > CONGESTION_TIMEOUT_US * overloadTimes_.load()) {
        overloadTimes_.fetch_add(1);
        std::vector<uint64_t> timeoutVec = {};
        ReportTimeout(timeoutVec);
    }
}

void QueueHandler::ReportTimeout(const std::vector<uint64_t>& timeoutTaskId)
{
    std::stringstream ss;
    ss << "Queue_Schedule_Timeout, queueId=" << GetQueueId() << ", timeout task gid: ";
    for (auto& id : timeoutTaskId) {
        ss << id << " ";
    }

    FFRT_LOGE("%s", ss.str().c_str());
    ffrt_task_timeout_cb func = ffrt_task_timeout_get_cb();
    if (func) {
        func(GetQueueId(), ss.str().c_str(), ss.str().size());
    }
}
} //namespace ffrt
