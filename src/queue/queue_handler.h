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
#ifndef FFRT_QUEUE_HANDLER_H
#define FFRT_QUEUE_HANDLER_H

#include <atomic>
#include <memory>
#include <string>
#include "c/queue.h"
#include "c/queue_ext.h"
#include "cpp/task.h"
#include "base_queue.h"

namespace ffrt {
class QueueTask;
class SerialQueue;
class Loop;

class SerialHandler {
public:
    SerialHandler(const char* name, const ffrt_queue_attr_t* attr, const int type = ffrt_queue_serial);
    ~SerialHandler();

    void Cancel();
    int Cancel(const char* name);
    int Cancel(QueueTask* task);
    void Dispatch(QueueTask* task);
    void Submit(QueueTask* task);
    void TransferTask(QueueTask* task);

    std::string GetDfxInfo() const;

    bool SetLoop(Loop* loop);
    bool ClearLoop();

    QueueTask* PickUpTask();
    uint64_t GetNextTimeout();

    inline bool IsValidForLoop()
    {
        return !isUsed_.load() && queue_->GetQueueType() == ffrt_queue_concurrent;
    }

    inline std::string GetName()
    {
        return name_;
    }

    inline uint32_t GetQueueId()
    {
        return queueId_;
    }

    inline bool HasTask(const char* name)
    {
        FFRT_COND_DO_ERR((queue_ == nullptr), return false, "[queueId=%u] constructed failed", queueId_);
        return queue_->HasTask(name);
    }

    bool IsIdle();
    void SetEventHandler(void* eventHandler);
    void* GetEventHandler();

    int Dump(const char* tag, char* buf, uint32_t len, bool historyInfo = true);
    int DumpSize(ffrt_inner_queue_priority_t priority);

private:
    void Deliver();
    void TransferInitTask();
    void SetTimeoutMonitor(QueueTask* task);
    void RunTimeOutCallback(QueueTask* task);

    // queue info
    std::string name_;
    int qos_ = qos_default;
    const uint32_t queueId_;
    std::unique_ptr<BaseQueue> queue_ = nullptr;
    std::atomic_bool isUsed_ = false;

    // for timeout watchdog
    uint64_t timeout_ = 0;
    std::atomic_int delayedCbCnt_ = {0};
    ffrt_function_header_t* timeoutCb_ = nullptr;
};
} // namespace ffrt

#endif // FFRT_QUEUE_HANDLER_H
