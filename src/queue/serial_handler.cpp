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
#include "serial_handler.h"

#include <sstream>
#include "dfx/log/ffrt_log_api.h"
#include "queue_monitor.h"
#include "serial_task.h"
#include "serial_queue.h"
#include "sched/scheduler.h"
#include "ffrt_trace.h"
#include "internal_inc/osal.h"
#include "util/event_handler_adapter.h"

namespace {
constexpr uint32_t STRING_SIZE_MAX = 128;
constexpr uint32_t TASK_DONE_WAIT_UNIT = 10;
constexpr uint64_t PROCESS_NAME_BUFFER_LENGTH = 1024;
std::atomic_uint32_t queueId(0);
}
namespace ffrt {
SerialHandler::SerialHandler(const char* name, const ffrt_queue_attr_t* attr, const ffrt_queue_type_t type)
    : queueId_(queueId++), queueType_(type)
{
    int maxConcurrency = 1;
    // parse queue attribute
    if (attr) {
        maxConcurrency = (ffrt_queue_attr_get_max_concurrency(attr))
            <= 0 ? 1 : ffrt_queue_attr_get_max_concurrency(attr);
        qos_ = (ffrt_queue_attr_get_qos(attr) >= ffrt_qos_background) ? ffrt_queue_attr_get_qos(attr) : qos_;
        timeout_ = ffrt_queue_attr_get_timeout(attr);
        timeoutCb_ = ffrt_queue_attr_get_callback(attr);
    }

    // callback reference counting is to ensure life cycle
    if (timeout_ > 0 && timeoutCb_ != nullptr) {
        SerialTask* cbTask = GetSerialTaskByFuncStorageOffset(timeoutCb_);
        cbTask->IncDeleteRef();
    }

    queue_ = std::make_unique<SerialQueue>(queueId_, maxConcurrency, type);
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "[queueId=%u] constructed failed", queueId_);

    if (name != nullptr && std::string(name).size() <= STRING_SIZE_MAX) {
        name_ = "sq_" + std::string(name) + "_" + std::to_string(queueId_);
    } else {
        name_ += "sq_unnamed_" + std::to_string(queueId_);
        FFRT_LOGW("failed to set [queueId=%u] name due to invalid name or length.", queueId_);
    }

    QueueMonitor::GetInstance().RegisterQueueId(queueId_, this);
    FFRT_LOGI("construct %s succ", name_.c_str());
}

SerialHandler::~SerialHandler()
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "cannot destruct, [queueId=%u] constructed failed", queueId_);
    FFRT_LOGI("destruct %s enter", name_.c_str());
    // clear tasks in queue
    queue_->Stop();
    while (QueueMonitor::GetInstance().QueryQueueStatus(queueId_) || queue_->GetActiveStatus()) {
        std::this_thread::sleep_for(std::chrono::microseconds(TASK_DONE_WAIT_UNIT));
    }
    QueueMonitor::GetInstance().ResetQueueStruct(queueId_);

    // release callback resource
    if (timeout_ > 0) {
        // wait for all delayedWorker to complete.
        while (delayedCbCnt_.load() > 0) {
            this_task::sleep_for(std::chrono::microseconds(timeout_));
        }

        if (timeoutCb_ != nullptr) {
            SerialTask* cbTask = GetSerialTaskByFuncStorageOffset(timeoutCb_);
            cbTask->DecDeleteRef();
        }
    }
    FFRT_LOGI("destruct %s leave", name_.c_str());
}

bool SerialHandler::SetLoop(Loop* loop)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return false, "cannot set loop, [queueId=%u] constructed failed", queueId_);
    return queue_->SetLoop(loop);
}

bool SerialHandler::ClearLoop()
{
    return queue_->ClearLoop();
}

SerialTask* SerialHandler::PickUpTask()
{
    return queue_->Pull();
}

uint64_t SerialHandler::GetNextTimeout()
{
    return queue_->GetNextTimeout();
}

void SerialHandler::NormalSerialTaskSubmit(SerialTask* task)
{
    FFRT_COND_DO_ERR((task == nullptr), return, "input invalid, serial task is nullptr");

    // if qos not specified, qos of the queue is inherited by task
    if (task->GetQos() == qos_inherit) {
        task->SetQos(qos_);
    }

    FFRT_SERIAL_QUEUE_TASK_SUBMIT_MARKER(queueId_, task->gid);

    int ret = queue_->Push(task);
    if (ret == SUCC) {
        FFRT_LOGD("submit task[%lu] into %s", task->gid, name_.c_str());
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
        FFRT_LOGD("task [%llu] activate %s", task->gid, name_.c_str());
        TransferTask(task);
    } else {
        FFRT_LOGD("task [%llu] with delay [%llu] activate %s", task->gid, task->GetDelay(), name_.c_str());
        if (ret == INACTIVE) {
            queue_->Push(task);
        }
        TransferInitTask();
    }
}
#ifdef OHOS_STANDARD_SYSTEM
void SerialHandler::MainSerialTaskSubmit(SerialTask* task)
{
    FFRT_COND_DO_ERR((task == nullptr), return, "input invalid, serial task is nullptr");
    int prio = task->GetPriority();
    int delayUs = task->GetDelay();

    auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
    std::function<void()> mainFunc = [=]() {
        f->exec(f);
        f->destroy(f);
    };
    bool taskStatus = EventHandlerAdapter::Instance()->PostTask(
        eventHandler_, mainFunc, task->GetName(), delayUs / 1000, static_cast<Priority>(prio));
    FFRT_COND_DO_ERR((taskStatus == false), return, "post task fail");
}

void SerialHandler::WorkerSerialTaskSubmit(SerialTask* task)
{
    FFRT_COND_DO_ERR((task == nullptr), return, "input invalid, serial task is nullptr");
    int prio = task->GetPriority();
    int delayUs = task->GetDelay();

    auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
    std::function<void()> workerFunc = [=]() {
        f->exec(f);
        f->destroy(f);
    };

    bool taskStatus = EventHandlerAdapter::Instance()->PostTask(
        eventHandler_, workerFunc, task->GetName(), delayUs / 1000, static_cast<Priority>(prio));
    FFRT_COND_DO_ERR((taskStatus == false), return, "post task fail");
}
#endif

void SerialHandler::Submit(SerialTask* task)
{
    switch (handleType_) {
        case NORMAL_SERIAL_HANDLER:
            NormalSerialTaskSubmit(task);
            break;
        case MAINTHREAD_SERIAL_HANDLER:
#ifdef OHOS_STANDARD_SYSTEM
            MainSerialTaskSubmit(task);
#endif
            break;
        case WORKERTHREAD_SERIAL_HANDLER:
#ifdef OHOS_STANDARD_SYSTEM
            WorkerSerialTaskSubmit(task);
#endif
            break;
        default: {
            FFRT_LOGE("Unsupport serial handle type=%d.", handleType_);
            break;
        }
    }
}

int SerialHandler::Cancel(SerialTask* task)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return INACTIVE, "cannot cancel, [queueId=%u] constructed failed", queueId_);
    FFRT_COND_DO_ERR((task == nullptr), return INACTIVE, "input invalid, serial task is nullptr");

    int ret = SUCC;
    switch (handleType_) {
        case NORMAL_SERIAL_HANDLER:
            ret = queue_->Remove(task);
            break;
        case MAINTHREAD_SERIAL_HANDLER:
        case WORKERTHREAD_SERIAL_HANDLER:
#ifdef OHOS_STANDARD_SYSTEM
            EventHandlerAdapter::Instance()->RemoveTask(eventHandler_, task->GetName());
#endif
            break;
        default: {
            FFRT_LOGE("Unsupport serial handle type=%d.", handleType_);
            break;
        }
    }

    if (ret == SUCC) {
        FFRT_LOGD("cancel task[%llu] %s succ", task->gid, task->label.c_str());
        task->Notify();
        task->Destroy();
    } else {
        FFRT_LOGW("cancel task[%llu] %s failed, task may have been executed", task->gid, task->label.c_str());
    }
    return ret;
}

void SerialHandler::Dispatch(SerialTask* inTask)
{
    SerialTask* nextTask = nullptr;
    for (SerialTask* task = inTask; task != nullptr; task = nextTask) {
        // dfx watchdog
        SetTimeoutMonitor(task);
        QueueMonitor::GetInstance().UpdateQueueInfo(queueId_, task->gid);

        // run user task
        FFRT_LOGD("run task [gid=%llu], queueId=%u", task->gid, queueId_);
        auto f = reinterpret_cast<ffrt_function_header_t*>(task->func_storage);
        FFRT_SERIAL_QUEUE_TASK_EXECUTE_MARKER(task->gid);
        f->exec(f);
        f->destroy(f);
        task->Notify();

        // run task batch
        nextTask = task->GetNextTask();
        if (nextTask == nullptr) {
            QueueMonitor::GetInstance().ResetQueueInfo(queueId_);
            if (!queue_->IsOnLoop()) {
                Deliver();
            }
        }
        task->DecDeleteRef();
    }
}

void SerialHandler::Deliver()
{
    SerialTask* task = queue_->Pull();
    if (task != nullptr) {
        TransferTask(task);
    }
}

void SerialHandler::TransferTask(SerialTask* task)
{
    auto entry = &task->fq_we;
    FFRTScheduler* sch = FFRTScheduler::Instance();
    if (!sch->InsertNode(&entry->node, QoS(task->GetQos()))) {
        FFRT_LOGE("failed to insert task [%llu] into %s", task->gid, queueId_, name_.c_str());
        return;
    }
}

void SerialHandler::TransferInitTask()
{
    std::function<void()> initFunc = []{};
    auto f = create_function_wrapper(initFunc, ffrt_function_kind_queue);
    SerialTask* initTask = GetSerialTaskByFuncStorageOffset(f);
    new (initTask)ffrt::SerialTask(this);
    initTask->SetQos(qos_);
    TransferTask(initTask);
}

void SerialHandler::SetTimeoutMonitor(SerialTask* task)
{
    if (timeout_ <= 0) {
        return;
    }

    task->IncDeleteRef();
    WaitUntilEntry* we = new (SimpleAllocator<WaitUntilEntry>::allocMem()) WaitUntilEntry();
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

void SerialHandler::RunTimeOutCallback(SerialTask* task)
{
    std::stringstream ss;
    ss << "serial queue [" << name_ << "] queueId=" << queueId_ << ", serial task gid=" << task->gid <<
        " execution time exceeds " << timeout_ << " us";
    std::string msg = ss.str();
    std::string eventName = "SERIAL_TASK_TIMEOUT";

#ifdef FFRT_SEND_EVENT
    time_t cur_time = time(nullptr);
    std::string sendMsg = std::string((ctime(&cur_time) == nullptr) ? "" : ctime(&cur_time)) + "\n" + msg + "\n";
    HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::FFRT, eventName,
        OHOS::HiviewDFX::HiSysEvent::EventType::FAULT, "PID", getpid(), "TGID", getgid(), "UID", getuid(),
        "MODULE_NAME", "ffrt", "PROCESS_NAME", "ffrt", "MSG", sendMsg);
#endif

    FFRT_LOGE("[%s], %s", eventName.c_str(), msg.c_str());
    if (timeoutCb_ != nullptr) {
        timeoutCb_->exec(timeoutCb_);
    }
}

std::string SerialHandler::GetDfxInfo() const
{
    std::stringstream ss;
    ss << " queue name [" << name_ << "]";
    if (queue_ != nullptr) {
        ss << ", remaining tasks count=" << queue_->GetMapSize();
    }
    return ss.str();
}
} // namespace ffrt
