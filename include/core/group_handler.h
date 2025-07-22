/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FFRT_GROUP_HANDLER_H
#define FFRT_GROUP_HANDLER_H
#include <string>
#include "c/type_def_ext.h"
#include "cpp/task_ext.h"
#include "tm/xpueu_task.h"

class GroupHandler {
public:
    GroupHandler() = default;
    virtual ~GroupHandler() = default;

    inline void Start()
    {
        ffrt::task_attr attr = {};
        reinterpret_cast<ffrt::task_attr_private *>(&attr)->groupRoot_ = true;
        groupRoot_ = ffrt::submit_h([this] { GroupRootFunc(); }, {}, {}, attr);
    }

    inline void Stop()
    {
        std::unique_lock<ffrt::mutex> lk(mutex_);
        isExit_.store(true);
        cv_.notify_one();
    }

    void GroupRootFunc()
    {
        std::unique_lock<ffrt::mutex> lk(mutex_);
        if (!isExit_.load()) {
            cv_.wait(lk);
        }

        ffrt::wait();
        ClearTask();
        delete this;
    }

    void AddTask(ffrt::XPUEUTask* task)
    {
        std::unique_lock<ffrt::mutex> lk(mutex_);
        task->IncDeleteRef();
        groupTasks_.push_back(task);
    }

    void ClearTask()
    {
        for (auto it = groupTasks_.rbegin(); it != groupTasks_.rend(); ++it) {
            ffrt::XPUEUTask* task = *it;
            task->DecDeleteRef();
        }
        groupTasks_.clear();
    }

    ffrt::CPUEUTask* GetGroupRootTask()
    {
        ffrt_task_handle_t p = groupRoot_;
        return reinterpret_cast<ffrt::CPUEUTask*>(p);
    }

private:
    std::atomic_bool isExit_ = {0};
    ffrt::mutex mutex_;
    ffrt::condition_variable cv_;
    ffrt::task_handle groupRoot_ = nullptr;
    std::vector<ffrt::XPUEUTask*> groupTasks_;
};

#endif // FFRT_GROUP_HANDLER_H