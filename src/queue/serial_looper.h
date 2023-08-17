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
#ifndef FFRT_SERIAL_LOOPER_H
#define FFRT_SERIAL_LOOPER_H

#include <atomic>
#include <memory>
#include <string>
#include "cpp/task.h"
#include "internal_inc/non_copyable.h"
#include "serial_queue.h"
#include "serial_task.h"

namespace ffrt {
inline SerialTask* GetSerialTaskByFuncStorageOffset(ffrt_function_header_t* f)
{
    return reinterpret_cast<SerialTask*>(static_cast<uintptr_t>(static_cast<size_t>(reinterpret_cast<uintptr_t>(f)) -
        (reinterpret_cast<size_t>(&((reinterpret_cast<SerialTask*>(0))->func_storage)))));
}

class SerialLooper : public NonCopyable {
public:
    SerialLooper(const char* name, int qos, uint64_t timeout = 0, ffrt_function_header_t* timeoutCb = nullptr);
    ~SerialLooper();
    
    void Quit();
    inline uint32_t GetQueueId() const
    {
        return qid_;
    }
    inline std::shared_ptr<SerialQueue> GetQueueIns() const
    {
        return queue_;
    }

private:
    void Run();
    void SetTimeoutMonitor(ITask* task);
    void RunTimeOutCallback(ITask* task);

    std::string name_ = "serial_queue_";
    std::atomic_bool isExit_ = {false};
    task_handle handle;
    std::shared_ptr<SerialQueue> queue_;
    // for timeout watchdog
    const uint32_t qid_;
    const uint64_t timeout_;
    ffrt_function_header_t* timeoutCb_;
    std::atomic_int delayedCbCnt_ = 0;
};
} // namespace ffrt

#endif // FFRT_SERIAL_LOOPER_H
