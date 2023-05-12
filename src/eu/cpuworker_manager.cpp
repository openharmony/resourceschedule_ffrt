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

#include <climits>
#include <cstring>
#include <sys/stat.h>
#include "eu/cpu_monitor.h"
#include "eu/cpu_manager_interface.h"
#include "eu/osattr_manager.h"
#include "sched/scheduler.h"
#include "sched/workgroup_internal.h"
#include "internal_inc/config.h"
#include "eu/qos_interface.h"
#include "eu/cpuworker_manager.h"

namespace ffrt {

bool CPUWorkerManager::IncWorker(const QoS& qos)
{
    std::unique_lock lock(groupCtl[qos()].tgMutex);
    if (tearDown) {
        return false;
    }

    auto worker = std::unique_ptr<WorkerThread>(new (std::nothrow) CPUWorker(qos, {
        std::bind(&CPUWorkerManager::PickUpTask, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::NotifyTaskPicked, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerIdleAction, this, std::placeholders::_1),
        std::bind(&CPUWorkerManager::WorkerRetired, this, std::placeholders::_1),
    }));
    if (worker == nullptr) {
        FFRT_LOGE("Inc CPUWorker: create worker\n");
        return false;
    }
    WorkerSetup(worker.get(), qos);
    groupCtl[qos()].threads[worker.get()] = std::move(worker);
    return true;
}

void CPUWorkerManager::WorkerSetup(WorkerThread* thread, const QoS& qos)
{
    if (qos() == qos_defined_ive) {
        SetTidToCGroup(cpuctlGroupIvePath, cpuThreadNode, thread->Id());
        SetTidToCGroup(cpusetGroupIvePath, cpuThreadNode, thread->Id());
    }

    pthread_setname_np(thread->GetThread().native_handle(), ("ffrtwk/CPU-" + (std::to_string(qos()))+ "-" +
        std::to_string(GlobalConfig::Instance().getQosWorkers()[static_cast<int>(qos())].size())).c_str());
    GlobalConfig::Instance().setQosWorkers(qos, thread->Id());
    if (qos() != qos_defined_ive) {
        QosApplyForOther(qos(), thread->Id());
        FFRT_LOGD("qos apply tid[%d] level[%d]\n", thread->Id(), qos());
    }
    WorkerJoinTg(qos, thread->Id());
}

void CPUWorkerManager::WakeupWorkers(const QoS& qos)
{
    if (tearDown) {
        return;
    }

    auto& ctl = sleepCtl[qos()];
    ctl.cv.notify_one();
}

int CPUWorkerManager::GetTaskCount(const QoS& qos)
{
    auto& sched = FFRTScheduler::Instance()->GetScheduler(qos);
    return sched.RQSize();
}

TaskCtx* CPUWorkerManager::PickUpTask(WorkerThread* thread)
{
    if (tearDown) {
        return nullptr;
    }

    auto& sched = FFRTScheduler::Instance()->GetScheduler(thread->GetQos());
    auto lock = GetSleepCtl(static_cast<int>(thread->GetQos()));
    std::lock_guard lg(*lock);
    return sched.PickNextTask();
}

void CPUWorkerManager::NotifyTaskPicked(const WorkerThread* thread)
{
    monitor.Notify(thread->GetQos(), TaskNotifyType::TASK_PICKED);
}

void CPUWorkerManager::WorkerRetired(WorkerThread* thread)
{
    pid_t pid = thread->Id();
    int qos = static_cast<int>(thread->GetQos());
    thread->SetExited(true);
    thread->Detach();

    {
        std::unique_lock lock(groupCtl[qos].tgMutex);
        auto worker = std::move(groupCtl[qos].threads[thread]);
        size_t ret = groupCtl[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(qos, pid);
        worker = nullptr;
    }
}

WorkerAction CPUWorkerManager::WorkerIdleAction(const WorkerThread* thread)
{
    if (tearDown) {
        return WorkerAction::RETIRE;
    }

    auto& ctl = sleepCtl[thread->GetQos()];
    std::unique_lock lk(ctl.mutex);
    monitor.IntoSleep(thread->GetQos());
    FFRT_LOGI("worker sleep");
#if defined(IDLE_WORKER_DESTRUCT)
    if (ctl.cv.wait_for(lk, std::chrono::seconds(5),
        [this, thread] {return tearDown || GetTaskCount(thread->GetQos());})) {
        monitor.WakeupCount(thread->GetQos());
        FFRT_LOGI("worker awake");
        return WorkerAction::RETRY;
    } else {
        monitor.TimeoutCount(thread->GetQos());
        FFRT_LOGI("worker exit");
        return WorkerAction::RETIRE;
    }
#else /* !IDLE_WORKER_DESTRUCT */
    ctl.cv.wait(lk, [this, thread] {return tearDown || GetTaskCount(thread->GetQos());});
    monitor.WakeupCount(thread->GetQos());
    FFRT_LOGI("worker awake");
    return WorkerAction::RETRY;
#endif /* IDLE_WORKER_DESTRUCT */
}

void CPUWorkerManager::NotifyTaskAdded(enum qos qos)
{
    QoS taskQos(qos);
    monitor.Notify(taskQos, TaskNotifyType::TASK_ADDED);
}

CPUWorkerManager::CPUWorkerManager() : monitor({
    std::bind(&CPUWorkerManager::IncWorker, this, std::placeholders::_1),
    std::bind(&CPUWorkerManager::WakeupWorkers, this, std::placeholders::_1),
    std::bind(&CPUWorkerManager::GetTaskCount, this, std::placeholders::_1)})
{
    groupCtl[qos_deadline_request].tg = std::unique_ptr<ThreadGroup>(new ThreadGroup());
}

void CPUWorkerManager::WorkerJoinTg(const QoS& qos, pid_t pid)
{
    if (qos == qos_user_interactive) {
        (void)JoinWG(pid);
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Join(pid);
}

void CPUWorkerManager::WorkerLeaveTg(const QoS& qos, pid_t pid)
{
    if (qos == qos_user_interactive) {
        return;
    }
    auto& tgwrap = groupCtl[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Leave(pid);
}

void CPUWorkerManager::SetTidToCGroup(const std::string &path, const std::string &name, int32_t pid)
{
    const std::string filename = path + name;
    char filePath[PATH_MAX_LENS] = {0};
    constexpr int32_t maxThreadId = 0xffff;
    if (pid <= 0 || pid >= maxThreadId) {
        return;
    }

    if (filename.empty()) {
        FFRT_LOGE("invalid para,filename is empty");
        return;
    }

    if ((strlen(filename.c_str()) > PATH_MAX_LENS) || (realpath(filename.c_str(), filePath) == nullptr)) {
        FFRT_LOGE("invalid file path:%s, error:%s\n", filename.c_str(), strerror(errno));
        return;
    }

    int32_t fd = open(filePath, O_RDWR);
    if (fd < 0) {
        FFRT_LOGE("[%d] fail to open cgroup ProcPath:%s", __LINE__, filePath);
        return;
    }

    const std::string pidStr = std::to_string(pid);
    int32_t ret = write(fd, pidStr.c_str(), pidStr.size());
    if (ret < 0) {
        FFRT_LOGE("[%d]fail to write path:%s pidStr:%s to fd:%d, errno:%d", __LINE__,
            filePath, pidStr.c_str(), fd, errno);
        close(fd);
        return;
    }
    const uint32_t bufferLen = 20;
    std::array<char, bufferLen> buffer {};
    int32_t count = read(fd, buffer.data(), bufferLen);
    if (count <= 0) {
        FFRT_LOGE("[%d]fail to read valueStr:%s to fd:%d, errno:%d", __LINE__, buffer.data(), fd, errno);
    } else {
        FFRT_LOGE("[%d]success to read %s buffer:%s", __LINE__, filePath, buffer.data());
    }
    close(fd);
}
} // namespace ffrt
