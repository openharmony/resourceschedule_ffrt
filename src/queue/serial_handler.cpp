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
#include "c/queue.h"
#include "dfx/log/ffrt_log_api.h"
#include "queue_monitor.h"
#include "serial_task.h"
#include "serial_queue.h"
#include "sched/scheduler.h"
#include "ffrt_trace.h"

namespace {
constexpr uint32_t STRING_SIZE_MAX = 128;
constexpr uint32_t TASK_DONE_WAIT_UNIT = 10;
std::atomic_uint32_t queueId(0);
}
namespace ffrt {
SerialHandler::SerialHandler(const char* name, const ffrt_queue_attr_t* attr) : queueId_(queueId++)
{
    queue_ = std::make_unique<SerialQueue>(queueId_);
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "[queueId=%u] constructed failed", queueId_);

    if (name != nullptr && std::string(name).size() <= STRING_SIZE_MAX) {
        name_ = "sq_" + std::string(name) + "_" + std::to_string(queueId_);
    } else {
        name_ += "sq_unnamed_" + std::to_string(queueId_);
        FFRT_LOGW("failed to set [queueId=%u] name due to invalid name or length.", queueId_);
    }

    // parse queue attribute
    if (attr) {
        qos_ = (ffrt_queue_attr_get_qos(attr) >= ffrt_qos_background) ? ffrt_queue_attr_get_qos(attr) : qos_;
        timeout_ = ffrt_queue_attr_get_timeout(attr);
        timeoutCb_ = ffrt_queue_attr_get_callback(attr);
    }

    // callback reference counting is to ensure life cycle
    if (timeout_ > 0 && timeoutCb_ != nullptr) {
        SerialTask* cbTask = GetSerialTaskByFuncStorageOffset(timeoutCb_);
        cbTask->IncDeleteRef();
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
        while (delayedCbCnt.load() > 0) {
            this_task::sleep_for(std::chrono::microseconds(timeout_));
        }

        if (timeoutCb_ != nullptr) {
            SerialTask* cbTask = GetSerialTaskByFuncStorageOffset(timeoutCb_);
            cbTask->DecDeleteRef();
        }
    }
    FFRT_LOGI("destruct %s leave", name_.c_str());
}

void SerialHandler::Submit(SerialTask* task)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return, "cannot submit, [queueId=%u] constructed failed", queueId_);
    FFRT_COND_DO_ERR((task == nullptr), return, "input invalid, serial task is nullptr");

    // if qos not specified, qos of the queue is inherited by task
    if (task->GetQos() == qos_inherit) {
        task->SetQos(qos_);
    }

    FFRT_SERIAL_QUEUE_TASK_SUBMIT_MARKER(queueId_, task->gid);

    int ret = queue_->Push(task);
    if (ret != INACTIVE) {
        FFRT_LOGD("submit task[%lu] into %s", task->gid, name_.c_str());
        return;
    }


    // active queue
    if (task->GetDelay() == 0) {
        TransferTask(task);
        FFRT_LOGD("task [%llu] activate %s", task->gid, name_.c_str());
    } else {
        queue_->Push(task);
        TransferInitTask();
        FFRT_LOGD("task [%llu] with delay [%llu] activate %s", task->gid, task->GetDelay(), name_.c_str());
    }
}

int SerialHandler::Cancel(SerialTask* task)
{
    FFRT_COND_DO_ERR((queue_ == nullptr), return INACTIVE, "cannot canel, [queueId=%u] constructed failed", queueId_);
    FFRT_COND_DO_ERR((task == nullptr), return INACTIVE, "input invalid, serial task is nullptr");

    int ret = queue_->Remove(task);
    if (ret == SUCC) {
        FFRT_LOGD("cancel task[%llu] %s succ", task->gid, task->label.c_str());
        task->Notify();
        task->Destroy();
    } else {
        FFRT_LOGW("cancel task[%llu] %s failed, task may have been executed", task->gid, task->label.c_str());
    }
    return ret;
}


} // namespace ffrt
