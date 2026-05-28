/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "sched/task_scheduler.h"
#include <random>
#include "eu/execute_unit.h"
#include "util/ffrt_facade.h"

namespace {
constexpr int UV_TASK_MAX_CONCURRENCY = 8;
} // namespace

namespace ffrt {
bool TaskScheduler::PushUVTaskToWaitingQueue(UVTask* task)
{
    std::lock_guard lg(uvMtx);
    if (uvTaskConcurrency_ >= UV_TASK_MAX_CONCURRENCY) {
        uvTaskWaitingQueue_.push_back(task);
        return true;
    }

    return false;
}

bool TaskScheduler::CheckUVTaskConcurrency(UVTask* task)
{
    std::lock_guard lg(uvMtx);
    // the number of workers executing UV tasks has reached the upper limit.
    // therefore, the current task is placed back to the head of the waiting queue (be preferentially obtained later).
    if (uvTaskConcurrency_ >= UV_TASK_MAX_CONCURRENCY) {
        uvTaskWaitingQueue_.push_front(task);
        return false;
    }

    uvTaskConcurrency_++;
    return true;
}

UVTask* TaskScheduler::PickWaitingUVTask()
{
    std::lock_guard lg(uvMtx);
    if (uvTaskWaitingQueue_.empty()) {
        if (uvTaskConcurrency_ > 0) {
            uvTaskConcurrency_--;
        }
        return nullptr;
    }

    UVTask* task = uvTaskWaitingQueue_.front();
    uvTaskWaitingQueue_.pop_front();
    task->SetDequeued();
    return task;
}

bool TaskScheduler::CancelUVWork(ffrt_executor_task_t* uvWork)
{
    std::lock_guard lg(uvMtx);
    if (!reinterpret_cast<LinkedList*>(uvWork->wq)->InList()) {
        FFRT_SYSEVENT_LOGW("the task has been picked, or has not been inserted");
        return false;
    }

    auto iter = std::remove_if(uvTaskWaitingQueue_.begin(), uvTaskWaitingQueue_.end(), [this, uvWork](UVTask* task) {
        if (task->uvWork == uvWork) {
            // SetDequeued writes to user-allocated uvWork, we must ensure it wasn't previously cancelled.
            if (cancelSet_.find(uvWork) == cancelSet_.end()) {
                task->SetDequeued();
            }
            return true;
        }
        return false;
    });
    if (iter != uvTaskWaitingQueue_.end()) {
        uvTaskWaitingQueue_.erase(iter, uvTaskWaitingQueue_.end());
        if (cancelSet_.find(uvWork) != cancelSet_.end()) {
            // We erase uvWork from set which was cancelled before to avoid memory leak.
            cancelSet_.erase(uvWork);
            return false;
        }
        return true;
    }
    LinkedList::RemoveCur(reinterpret_cast<LinkedList*>(&uvWork->wq));
    return cancelSet_.insert(uvWork).second;
}

std::mutex* TaskScheduler::GetMutex()
{
    /* We use acquire on load and release on store to enforce the
     * happens-before relationship between the mutex implicit
     * initialization and the publication of its address.
     * i.e. if a thread reads the address of the mutex then
     * it has been already initialized by the thread that published
     * its address.
     */
    auto curMtx = mtx.load(std::memory_order_acquire);
    if (curMtx == nullptr) {
        curMtx = &FFRTFacade::GetEUInstance().GetWorkerGroup(qos).mutex;
        mtx.store(curMtx, std::memory_order_release);
    }
    return curMtx;
}

SchedulerFactory &SchedulerFactory::Instance()
{
    static SchedulerFactory fac;
    return fac;
}
} // namespace ffrt