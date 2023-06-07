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
#include "serial_task.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/slab.h"

namespace ffrt {
SerialTask::SerialTask()
{
    FFRT_LOGD("ctor serial task [0x%x]", this);
}

SerialTask::~SerialTask()
{
    FFRT_LOGD("dtor serial task [0x%x]", this);
}

ITask* SerialTask::SetQueHandler(IHandler* handler)
{
    handler_ = handler;
    return this;
}

void SerialTask::Wait()
{
    std::unique_lock lock(mutex_);
    while (!isFinished_) {
        cond_.wait(lock);
    }
}

void SerialTask::Notify()
{
    std::unique_lock lock(mutex_);
    isFinished_ = true;
    cond_.notify_all();
}

void SerialTask::freeMem()
{
    SimpleAllocator<SerialTask>::freeMem(this);
}
} // namespace ffrt
