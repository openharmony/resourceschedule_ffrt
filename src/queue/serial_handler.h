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

#include "c/queue.h"
#include "cpp/task.h"
#include "ihandler.h"

namespace ffrt {
class SerialTask;
class SerialQueue;
class Loop;
enum HandlerType {
    NORMAL_SERIAL_HANDLER = 0,
    MAINTHREAD_SERIAL_HANDLER,
    WORKERTHREAD_SERIAL_HANDLER,
};

class SerialHandler : public IHandler {
public:
    SerialHandler(const char* name, const ffrt_queue_attr_t* attr, const ffrt_queue_type_t type = ffrt_queue_serial);
    ~SerialHandler() override;

    int Cancel(SerialTask* task) override;
    void Dispatch(SerialTask* task) override;
    void Submit(SerialTask* task) override;
    void TransferTask(SerialTask* task) override;

    std::string GetDfxInfo() const;

    bool SetLoop(Loop* loop);
    bool ClearLoop();

    SerialTask* PickUpTask();
    uint64_t GetNextTimeout();

    inline bool IsValidForLoop()
    {
        return !isUsed_.load() && queueType_ == ffrt_queue_concurrent;
    }

    inline std::string GetName() override
    {
        return name_;
    }

    inline uint32_t GetQueueId() override
    {
        return queueId_;
    }

    inline void SetHandlerType(HandlerType type)
    {
        handleType_ = type;
    }

    inline void SetEventHandler(void* eventHandler)
    {
        eventHandler_ = eventHandler;
    }

    inline void* GetEventHandler()
    {
        return eventHandler_;
    }

private:
    void Deliver();
    void TransferInitTask();
    void SetTimeoutMonitor(SerialTask* task);
    void RunTimeOutCallback(SerialTask* task);

    void NormalSerialTaskSubmit(SerialTask* task);
    void MainSerialTaskSubmit(SerialTask* task);
    void WorkerSerialTaskSubmit(SerialTask* task);

    // queue info
    std::string name_;
    int qos_ = qos_default;
    const uint32_t queueId_;
    std::unique_ptr<SerialQueue> queue_;
    std::atomic_bool isUsed_ = false;

    // for timeout watchdog
    uint64_t timeout_ = 0;
    std::atomic_int delayedCbCnt_ = {0};
    ffrt_function_header_t* timeoutCb_ = nullptr;

    HandlerType handleType_ = NORMAL_SERIAL_HANDLER;
    ffrt_queue_type_t queueType_ = ffrt_queue_serial;
    void* eventHandler_;
};
} // namespace ffrt

#endif // FFRT_SERIAL_HANDLER_H
