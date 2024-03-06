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
#ifndef FFRT_SERIAL_HANDLER_H
#define FFRT_SERIAL_HANDLER_H

#include <atomic>
#include <memory>
#include <string>

#include "cpp/task.h"
#include "ihandler.h"

namespace ffrt {
class SerialTask;
class SerialQueue;
class SerialHandler : public IHandler {
public:
    SerialHandler(const char* name, const ffrt_queue_attr_t* attr);
    ~SerialHandler() override;

    int Cancel(SerialTask* task) override;
    void Dispatch(SerialTask* task) override;
    void Submit(SerialTask* task) override;
    void TransferTask(SerialTask* task) override;

    std::string GetDfxInfo() const;

    inline std::string GetName() override
    {
        return name_;
    }

    inline uint32_t GetQueueId() override
    {
        return queueId_;
    }

private:
    void Deliver();
    void TransferInitTask();
    void SetTimeoutMonitor(SerialTask* task);
    void RunTimeOutCallback(SerialTask* task);

    // queue info
    std::string name_;
    int qos_ = qos_default;
    const uint32_t queueId_;
    std::unique_ptr<SerialQueue> queue_;

    // for timeout watchdog
    uint64_t timeout_ = 0;
    std::atomic_int delayedCbCnt_ = {0};
    ffrt_function_header_t* timeoutCb_ = nullptr;
};
} // namespace ffrt

#endif // FFRT_SERIAL_HANDLER_H
