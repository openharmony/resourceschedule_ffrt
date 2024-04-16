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

namespace ffrt {

FFRTScheduler* FFRTScheduler::Instance()
{
    return &SingletonRegister<FFRTScheduler>::Instance();
}

void FFRTScheduler::RegistInsCb(SingleInsCB<FFRTScheduler>::Instance &&cb)
{
    SingletonRegister<FFRTScheduler>::RegistInsCb(std::move(cb));
}

#ifdef FFRT_IO_TASK_SCHEDULER
void FFRTScheduler::PushTask(CPUEUTask* task)
{
    int level = task->qos();
    auto lock = ExecuteUnit::Instance().GetSleepCtl(level);
    lock->lock();
    fifoQue[static_cast<unsigned short>(level)]->WakeupTask(task);
    lock->unlock();
    ExecuteUnit::Instance().NotifyTaskAdded(QoS(level));
}
#endif

bool FFRTScheduler::InsertNode(LinkedList* node, const QoS qos)
{
    if (node == nullptr) {
        return false;
    }

    int level = qos();
    if (level == qos_inherit) {
        return false;
    }

    ffrt_executor_task_t* task = reinterpret_cast<ffrt_executor_task_t*>(reinterpret_cast<char*>(node) -
        offsetof(ffrt_executor_task_t, wq));
    uintptr_t taskType = task->type;

    auto lock = ExecuteUnit::Instance().GetSleepCtl(level);
    lock->lock();
    fifoQue[static_cast<unsigned short>(level)]->WakeupNode(node);
    lock->unlock();

#ifdef FFRT_IO_TASK_SCHEDULER
    if (taskType == ffrt_io_task) {
        ExecuteUnit::Instance().NotifyLocalTaskAdded(qos);
        return true;
    }
#endif

    ExecuteUnit::Instance().NotifyTaskAdded(qos);
    return true;
}

bool FFRTScheduler::RemoveNode(LinkedList* node, const QoS qos)
{
    if (node == nullptr) {
        return false;
    }

    int level = qos();
    if (level == qos_inherit) {
        return false;
    }
    auto lock = ExecuteUnit::Instance().GetSleepCtl(level);
    lock->lock();
    if (!node->InList()) {
        lock->unlock();
        return false;
    }
    fifoQue[static_cast<unsigned short>(level)]->RemoveNode(node);
    lock->unlock();
#ifdef FFRTT_BBOX_ENABLE
    TaskFinishCounterInc();
#endif
    return true;
}

bool FFRTScheduler::WakeupTask(CPUEUTask* task)
{
    int qos_level = static_cast<int>(qos_default);
    if (task != nullptr) {
        qos_level = task->qos();
        if (qos_level == qos_inherit) {
            return false;
        }
    }
    QoS _qos = QoS(qos_level);
    int level = _qos();
    auto lock = ExecuteUnit::Instance().GetSleepCtl(level);
    lock->lock();
    fifoQue[static_cast<unsigned short>(level)]->WakeupTask(task);
    lock->unlock();
    FFRT_LOGD("qos[%d] task[%lu] entered q", level, task->gid);
    ExecuteUnit::Instance().NotifyTaskAdded(_qos);
    return true;
}

} // namespace ffrt