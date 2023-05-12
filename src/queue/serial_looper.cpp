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
#include "serial_looper.h"
#include "cpp/task.h"
#include "dfx/log/ffrt_log_api.h"
#include "queue/ihandler.h"

namespace {
constexpr uint32_t STRING_SIZE_MAX = 128;
}

namespace ffrt {
SerialLooper::SerialLooper(const char* name, enum qos qos)
{
    if (name != nullptr && (std::string(name).size() <= STRING_SIZE_MAX)) {
        name_ += name;
    }

    m_queue = std::make_shared<SerialQueue>(name_);
    FFRT_COND_TRUE_DO_ERR((m_queue == nullptr), "failed to construct SerialQueue", return);
    // using nested submission is to submit looper task on worker.
    // when ffrt::wait() is used in the current thread, the looper task is not in the waiting list.
    submit([this, qos] { handle = submit_h([this, qos] { Run(); }, {}, {}, task_attr().name(name_.c_str()).qos(qos)); },
        {}, { &handle });
    ffrt::wait({&handle});
    FFRT_COND_TRUE_DO_ERR((handle == nullptr), "failed to construct SerialLooper", return);
}

SerialLooper::~SerialLooper()
{
    Quit();
}

void SerialLooper::Quit()
{
    isExit_.store(true);
    m_queue->Quit();
    if (handle != nullptr) {
        wait({handle});
        handle = nullptr;
    }
}

void SerialLooper::Run()
{
    FFRT_LOGI("%s, loop enter", name_.c_str());
    while (!isExit_.load()) {
        ITask* task = m_queue->Next();
        FFRT_LOGD("get next task [0x%x]", task);
        if (task) {
            FFRT_COND_TRUE_DO_ERR((task->handler_ == nullptr), "failed to run task, handler is nullptr", break);
            task->handler_->DispatchTask(task);
        }
    }
    FFRT_LOGI("%s, looper leave", name_.c_str());
}
} // namespace ffrt
