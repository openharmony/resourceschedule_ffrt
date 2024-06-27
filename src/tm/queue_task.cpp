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
#include "queue_task.h"
#include "ffrt_trace.h"
#include "dfx/log/ffrt_log_api.h"
#include "c/task.h"
#include "util/slab.h"

namespace ffrt {
QueueTask::QueueTask(QueueHandler* handler, const task_attr_private* attr, bool insertHead)
    : handler_(handler), insertHead_(insertHead)
{
    type = ffrt_queue_task;
    if (handler) {
        if (attr) {
            label = handler->GetName() + "_" + attr->name_ + "_" + std::to_string(gid);
        } else {
            label = handler->GetName() + "_" + std::to_string(gid);
        }
    }

    fq_we.task = reinterpret_cast<CPUEUTask*>(this);
    uptime_ = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    if (attr) {
        delay_ = attr->delay_;
        qos_ = attr->qos_;
        uptime_ += delay_;
        prio_ = attr->prio_;
        stack_size = std::max(attr->stackSize_, MIN_STACK_SIZE);
    }

    FFRT_LOGD("ctor task [gid=%llu], delay=%lluus, type=%llu, prio=%u", gid, delay_, type, prio_);
}

QueueTask::~QueueTask()
{
    FFRT_LOGD("dtor task [gid=%llu]", gid);
}

void QueueTask::Destroy()
{
    // release user func
    auto f = reinterpret_cast<ffrt_function_header_t*>(func_storage);
    f->destroy(f);
    // free serial task object
    DecDeleteRef();
}

void QueueTask::Notify()
{
    FFRT_SERIAL_QUEUE_TASK_FINISH_MARKER(gid);
    std::unique_lock lock(mutex_);
    isFinished_.store(true);
    if (onWait_) {
        cond_.notify_all();
    }
}

void QueueTask::Execute()
{
    if (isFinished_.load()) {
        FFRT_LOGE("task [gid=%llu] is complete, no need to execute again", gid);
        return;
    }

    handler_->Dispatch(this);
    FFRT_TASKDONE_MARKER(gid);
}

void QueueTask::Wait()
{
    std::unique_lock lock(mutex_);
    onWait_ = true;
    while (!isFinished_.load()) {
        cond_.wait(lock);
    }
}

void QueueTask::FreeMem()
{
    SimpleAllocator<QueueTask>::FreeMem(this);
}

uint32_t QueueTask::GetQueueId() const
{
    return handler_->GetQueueId();
}
} // namespace ffrt
