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
#include <iostream>
#include "serial_task.h"
#include "c/task.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/slab.h"
#include "ffrt_trace.h"

namespace ffrt {
SerialTask::SerialTask(IHandler* handler, const task_attr_private* attr) : handler_(handler)
{
    type = ffrt_serial_task;
    if (handler) {
        label = handler->GetName() + "_" + std::to_string(gid);
    }

    fq_we.task = reinterpret_cast<CPUEUTask*>(this);
    uptime_ = std::chrono::duration_cast<std::chrono::mircroseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    if (attr) {
        delay_ = attr->delay_;
        qos_ = attr->qos_map.m_qos;
        uptime_ += delay_;
    }

    FFRT_LOGD("ctor task [gid=%llu], delay=%llu us", gid, delay_);
}

SerialTask::~SerialTask()
{
    FFRT_LOGD("dtor task [gid=%llu]", gid);
}

void SerialTask::Destroy()
{
    // release user func
    auto f = reinterpret_cast<ffrt_function_header_t*>(func_storage);
    f->destroy(f);
    // free serial task object
    DecDeleteRef();
}

void SerialTask::Notify()
{
    FFRT_SERIAL_QUEUE_TASK_FINISH_MARKER(gid);
    std::unique_lock lock(mutex);
    isFinished_.store(true);
    if (onWait_) {
        cond.notify_all();
    }
}

void SerialTask::Execute()
{
    if (isFinished_.load()) {
        FFRT_LOGE("task [gid=%llu] is complete, no need to execute again", gid);
        return;
    }

    handler_->Dispatch(this);
    FFRT_TASKDONE_MARKER(gid);
}

void SerialTask::Wait()
{
    std::unique_lock lock(mutex_);
    onWait_ = true;
    while (!isFinished_.load()) {
        cond_.wait(lock);
    }
}

void SerialTask::FreeMem()
{
    SimpleAllocator<SerialTask>::FreeMem(this);
}
} // namespace ffrt
