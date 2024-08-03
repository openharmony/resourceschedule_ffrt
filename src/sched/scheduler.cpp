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

#include "scheduler.h"

#include "util/singleton_register.h"

namespace {
constexpr int TASK_OVERRUN_THRESHOLD = 1000;
constexpr int TASK_OVERRUN_ALARM_FREQ = 500;
}

namespace ffrt {

FFRTScheduler* FFRTScheduler::Instance()
{
    return &SingletonRegister<FFRTScheduler>::Instance();
}

void FFRTScheduler::RegistInsCb(SingleInsCB<FFRTScheduler>::Instance &&cb)
{
    SingletonRegister<FFRTScheduler>::RegistInsCb(std::move(cb));
}

bool FFRTScheduler::InsertNode(LinkedList* node, const QoS qos)
{
    FFRT_COND_DO_ERR((node == nullptr), return false, "Node is NULL");

    int level = qos();
    FFRT_COND_DO_ERR((level == qos_inherit), return false, "Level incorrect");

    ffrt_executor_task_t* task = reinterpret_cast<ffrt_executor_task_t*>(reinterpret_cast<char*>(node) -
        offsetof(ffrt_executor_task_t, wq));
    uintptr_t taskType = task->type;

    if (taskType == ffrt_uv_task || taskType == ffrt_io_task) {
        FFRT_EXECUTOR_TASK_READY_MARKER(task); // uv/io task ready to enque
    }
    auto lock = ExecuteUnit::Instance().GetSleepCtl(level);
    lock->lock();
    fifoQue[static_cast<unsigned short>(level)]->WakeupNode(node);
    lock->unlock();

    if (taskType == ffrt_io_task) {
        ExecuteUnit::Instance().NotifyLocalTaskAdded(qos);
        return true;
    }

    ExecuteUnit::Instance().NotifyTaskAdded(qos);
    return true;
}

bool FFRTScheduler::RemoveNode(LinkedList* node, const QoS qos)
{
    FFRT_COND_DO_ERR((node == nullptr), return false, "Node is NULL");

    int level = qos();
    FFRT_COND_DO_ERR((level == qos_inherit), return false, "Level incorrect");
    
    auto lock = ExecuteUnit::Instance().GetSleepCtl(level);
    lock->lock();
    if (!node->InList()) {
        lock->unlock();
        return false;
    }
    fifoQue[static_cast<unsigned short>(level)]->RemoveNode(node);
    lock->unlock();
#ifdef FFRT_BBOX_ENABLE
    TaskFinishCounterInc();
#endif
    return true;
}

bool FFRTScheduler::WakeupTask(CPUEUTask* task)
{
    FFRT_COND_DO_ERR((task == nullptr), return false, "task is nullptr");

    int qosLevel = task->qos();
    if (qosLevel == qos_inherit) {
        FFRT_LOGE("qos inhert not support wake up task[%lu], type[%d], name[%s]",
            task->gid, task->type, task->label.c_str());
        return false;
    }

    QoS _qos = qosLevel;
    int level = _qos();
    uint64_t gid = task->gid;
    bool notifyWorker = task->notifyWorker_;
    std::string label = task->label;

    FFRT_READY_MARKER(gid); // ffrt normal task ready to enque
    auto lock = ExecuteUnit::Instance().GetSleepCtl(level);
    lock->lock();
    fifoQue[static_cast<unsigned short>(level)]->WakeupTask(task);
    int taskCount = fifoQue[static_cast<size_t>(level)]->RQSize();
    lock->unlock();

    // The ownership of the task belongs to ReadyTaskQueue, and the task cannot be accessed any more.
    FFRT_LOGD("qos[%d] task[%lu] entered q", level, gid);
    if (taskCount >= TASK_OVERRUN_THRESHOLD && taskCount % TASK_OVERRUN_ALARM_FREQ == 0) {
        FFRT_LOGW("qos [%d], task [%s] entered q, task count [%d] exceeds threshold.",
            level, label.c_str(), taskCount);
    }

    if (notifyWorker) {
        ExecuteUnit::Instance().NotifyTaskAdded(_qos);
    }

    return true;
}

} // namespace ffrt