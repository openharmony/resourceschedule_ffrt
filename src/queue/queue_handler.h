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
#include <unordered_map>
#include "c/queue.h"
#include "c/queue_ext.h"
#include "cpp/task.h"
#include "base_queue.h"
#include "sched/execute_ctx.h"

namespace ffrt {
class QueueTask;
class SerialQueue;
class Loop;

class QueueHandler {
public:
    QueueHandler(const char* name, const ffrt_queue_attr_t* attr, const int type = ffrt_queue_serial);
    ~QueueHandler();

    void Cancel();
    void CancelAndWait();
    int Cancel(const char* name);
    int Cancel(QueueTask* task);
    void Dispatch(QueueTask* inTask);
    void Submit(QueueTask* task);
    void TransferTask(QueueTask* task);

    std::string GetDfxInfo() const;

    bool SetLoop(Loop* loop);
    bool ClearLoop();
	
    QueueTask* PickUpTask();
	
	inline bool IsValidForLoop()
    {
        return !isUsed_.load() && (queue_->GetQueueType() == ffrt_queue_concurrent
				|| queue_->GetQueueType() == ffrt_queue_eventhandler_interactive);
    }
	
	inline std::string GetName()
    {
        return name_;
    }
	
	inline uint32_t GetQueueId()
    {
        FFRT_COND_DO_ERR((queue_ == nullptr), return 0, "queue construct failed");
        return queue_->GetQueueId();
    }

    inline uint32_t GetExecTaskId() const
    {
        return execTaskId_.load();
    }

    inline bool HasTask(const char* name)
    {
        FFRT_COND_DO_ERR((queue_ == nullptr), return false, "[queueId=%u] constructed failed", GetQueueId());
        return queue_->HasTask(name);
    }

    inline uint64_t GetTaskCnt()
    {
        FFRT_COND_DO_ERR((queue_ == nullptr), return false, "[queueId=%u] constructed failed", GetQueueId());
        return queue_->GetMapSize();
    }

    bool IsIdle();
    void SetEventHandler(void* eventHandler);
    void* GetEventHandler();

    int Dump(const char* tag, char* buf, uint32_t len, bool historyInfo = true);
    int DumpSize(ffrt_inner_queue_priority_t priority);

    inline const std::unique_ptr<BaseQueue>& GetQueue()
    {
        return queue_;
    }

private:
    void Deliver();
    void TransferInitTask();
    void SetTimeoutMonitor(QueueTask* task);
    void RunTimeOutCallback(QueueTask* task);

    void CheckOverload();
    void ReportTimeout(const std::vector<uint64_t>& timeoutTaskId);
    void CheckSchedDeadline();
    void SendSchedTimer(TimePoint delay);
    void AddSchedDeadline(QueueTask* task);
    void RemoveSchedDeadline(QueueTask* task);

    // queue info
    std::string name_;
    int qos_ = qos_default;
    std::unique_ptr<BaseQueue> queue_ = nullptr;
    std::atomic_bool isUsed_ = false;
    std::atomic_uint64_t execTaskId_ = 0;

    // for timeout watchdog
    uint64_t timeout_ = 0;
    std::atomic_int delayedCbCnt_ = {0};
    ffrt_function_header_t* timeoutCb_ = nullptr;

    std::mutex mutex_;
    bool initSchedTimer_ = false;
    WaitUntilEntry* we_ = nullptr;
    std::atomic_uint32_t overloadTimes_ = {1};
    std::unordered_map<QueueTask*, uint64_t> schedDeadline_;
};
} // namespace ffrt

#endif // FFRT_QUEUE_HANDLER_H
