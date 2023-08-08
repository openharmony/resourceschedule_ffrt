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

#include "eu/cpu_monitor.h"
#include <iostream>
#include <thread>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include "sync/poller.h"
#include "sched/scheduler.h"
#include "eu/wgcm.h"
#include "eu/execute_unit.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/config.h"
namespace ffrt {
void CPUMonitor::HandleBlocked(const QoS& qos)
{
    int taskCount = ops.GetTaskCount(qos);
    if (taskCount == 0) {
        return;
    }
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    size_t exeValue = static_cast<uint32_t>(workerCtrl.executionNum);
    workerCtrl.lock.unlock();
    size_t blockedNum = CountBlockedNum(qos);
    if (blockedNum > 0 && (exeValue - blockedNum < workerCtrl.maxConcurrency) && exeValue < workerCtrl.hardLimit) {
        Poke(qos);
    }
}

void MonitorMain(CPUMonitor* monitor)
{
    (void)pthread_setname_np(pthread_self(), "ffrt_moniotor");
    int ret = prctl(PR_WGCM_CTL, WGCM_CTL_SERVER_REG, 0, 0, 0);
    if (ret) {
        FFRT_LOGE("[SERVER] wgcm register server failed ret is %{public}d", ret);
    }

    ret = syscall(SYS_gettid);
    if (ret == -1) {
        monitor->monitorTid = 0;
        FFRT_LOGE("syscall(SYS_gettid) failed");
    } else {
        monitor->monitorTid = static_cast<uint32_t>(ret);
    }

    for (unsigned int i = 0; i < static_cast<unsigned int>(QoS::Max()); i++) {
        struct wgcm_workergrp_data grp = {0};
        grp.gid = i;
        grp.min_concur_workers = DEFAULT_MINCONCURRENCY;
        grp.max_workers_sum = DEFAULT_HARDLIMIT;
        ret = prctl(PR_WGCM_CTL, WGCM_CTL_SET_GRP, &grp, 0, 0);
        if (ret) {
            FFRT_LOGE("[SERVER] wgcm group %u register failed\n ret is %{public}d", i, ret);
        }
    }

    while (true) {
        struct wgcm_workergrp_data data = {0};
        ret = prctl(PR_WGCM_CTL, WGCM_CTL_WAIT, &data, 0, 0);
        if (ret) {
            FFRT_LOGE("[SERVER] wgcm server wait failed ret is %{public}d", ret);
            sleep(1);
            continue;
        }
        if (data.woken_flag == WGCM_ACTIVELY_WAKE) {
            break;
        }

        for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
            monitor->HandleBlocked(qos);
        }
    }
    ret = prctl(PR_WGCM_CTL, WGCM_CTL_UNREGISTER, 0, 0, 0);
    if (ret) {
        FFRT_LOGE("[SERVER] wgcm server unregister failed ret is %{public}d.", ret);
    }
}

void CPUMonitor::SetupMonitor()
{
    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        ctrlQueue[qos].hardLimit = DEFAULT_HARDLIMIT;
        ctrlQueue[qos].workerManagerID = static_cast<uint32_t>(qos);
        ctrlQueue[qos].maxConcurrency = GlobalConfig::Instance().getCpuWorkerNum(qos);
    }
}

void CPUMonitor::RegWorker(const QoS& qos)
{
    struct wgcm_workergrp_data grp;
    grp.gid = static_cast<uint32_t>(qos);
    grp.server_tid = monitorTid;
    int ret = prctl(PR_WGCM_CTL, WGCM_CTL_WORKER_REG, &grp, 0, 0);
    if (ret) {
        FFRT_LOGE("[WORKER] Register failed! error=%d\n", ret);
    }
}

void CPUMonitor::UnRegWorker()
{
    int ret = prctl(PR_WGCM_CTL, WGCM_CTL_UNREGISTER, 0, 0, 0);
    if (ret) {
        FFRT_LOGE("leave workgroup failed  error=%d\n", ret);
    }
}

CPUMonitor::CPUMonitor(CpuMonitorOps&& ops) : ops(ops)
{
    SetupMonitor();
    monitorThread = nullptr;
}

CPUMonitor::~CPUMonitor()
{
    if (monitorThread != nullptr) {
        monitorThread->join();
    }
    delete monitorThread;
    monitorThread = nullptr;
}

void CPUMonitor::StartMonitor()
{
    monitorThread = new std::thread(MonitorMain, this);
}

uint32_t CPUMonitor::GetMonitorTid() const
{
    return monitorTid;
}

void CPUMonitor::IncSleepingRef(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum++;
    workerCtrl.lock.unlock();
}

void CPUMonitor::DecSleepingRef(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.lock.unlock();
}

void CPUMonitor::DecExeNumRef(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.executionNum--;
    workerCtrl.lock.unlock();
}

size_t CPUMonitor::CountBlockedNum(const QoS& qos)
{
    struct wgcm_workergrp_data grp = {0};
    grp.gid = static_cast<uint32_t>(qos);
    grp.server_tid = monitorTid;
    int ret = prctl(PR_WGCM_CTL, WGCM_CTL_GET, &grp, 0, 0);
    if (ret) {
        FFRT_LOGE("failed to get wgcm count");
    } else {
        return static_cast<size_t>(grp.blk_workers_sum);
    }
    return 0;
}

void CPUMonitor::Notify(const QoS& qos, TaskNotifyType notifyType)
{
    int taskCount = ops.GetTaskCount(qos);
    FFRT_LOGD("qos[%d] task notify op[%d] cnt[%ld]", (int)qos, (int)notifyType, ops.GetTaskCount(qos));
    switch (notifyType) {
        case TaskNotifyType::TASK_ADDED:
            if (taskCount > 0) {
                Poke(qos);
            }
            break;
        case TaskNotifyType::TASK_PICKED:
            if (taskCount > 0) {
                Poke(qos);
            }
            break;
        case TaskNotifyType::TASK_LOCAL:
            if (taskCount < WakedWorkerNum(qos)) {
                break;
            }
            Poke(qos);
            break;
        default:
            break;
    }
}

void CPUMonitor::TimeoutCount(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.lock.unlock();
}

void CPUMonitor::WakeupCount(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.executionNum++;
    workerCtrl.lock.unlock();
}

int CPUMonitor::WakedWorkerNum(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    std::unique_lock lk(workerCtrl.lock);
    return workerCtrl.executionNum;
}

void CPUMonitor::IntoSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum++;
    workerCtrl.executionNum--;
    workerCtrl.lock.unlock();
}

void CPUMonitor::IntoPollWait(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.pollWaitFlag = true;
    workerCtrl.lock.unlock();
}

void CPUMonitor::OutOfPollWait(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.pollWaitFlag = false;
    workerCtrl.lock.unlock();
}

void CPUMonitor::Poke(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
#ifdef FFRT_IO_TASK_SCHEDULER
    if (workerCtrl.executionNum > 4 && ops.GetTaskCount(qos) < workerCtrl.executionNum) {
        workerCtrl.lock.unlock();
        return;
    }
#endif

    FFRT_LOGD("qos[%d] exe num[%d] slp num[%d]", (int)qos, workerCtrl.executionNum, workerCtrl.sleepingWorkerNum);
    if (static_cast<uint32_t>(workerCtrl.executionNum) < workerCtrl.maxConcurrency) {
        if (workerCtrl.sleepingWorkerNum == 0) {
            workerCtrl.executionNum++;
            workerCtrl.lock.unlock();
            ops.IncWorker(qos);
        } else {
            workerCtrl.lock.unlock();
            ops.WakeupWorkers(qos);
        }
    } else {
#ifdef FFRT_IO_TASK_SCHEDULER
        if (workerCtrl.pollWaitFlag) {
            PollerProxy::Instance()->GetPoller(qos).WakeUp();
        }
#endif
        workerCtrl.lock.unlock();
    }
}
}
