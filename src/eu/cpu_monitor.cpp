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
#include "sched/scheduler.h"
#include "eu/wgcm.h"
#include "eu/execute_unit.h"
#include "dfx/log/ffrt_log_api.h"
#include "internal_inc/config.h"
#include "util/name_manager.h"
#ifdef FFRT_IO_TASK_SCHEDULER
#include "sync/poller.h"
#include "util/spmc_queue.h"

namespace {
const int TIGGER_SUPPRESS_WORKER_COUNT = 4;
const int TIGGER_SUPPRESS_EXECUTION_NUM = 2;
}
#endif

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
        Poke(qos, TaskNotifyType::TASK_ADDED);
    }
}

void MonitorMain(CPUMonitor* monitor)
{
    (void)pthread_setname_np(pthread_self(), CPU_MONITOR_NAME);
    int ret = prctl(PR_WGCM_CTL, WGCM_CTL_SERVER_REG, 0, 0, 0);
    if (ret) {
        FFRT_LOGE("[SERVER] wgcm register server failed ret is %d", ret);
    }

    ret = syscall(SYS_gettid);
    if (ret == -1) {
        monitor->monitorTid = 0;
        FFRT_LOGE("syscall(SYS_gettid) failed");
    } else {
        monitor->monitorTid = static_cast<uint32_t>(ret);
    }

    for (unsigned int i = 0; i < static_cast<unsigned int>(QoS::Max()); i++) {
        struct wgcm_workergrp_data grp = {0, 0, 0, 0, 0, 0, 0, 0};
        grp.gid = i;
        grp.min_concur_workers = DEFAULT_MINCONCURRENCY;
        grp.max_workers_sum = DEFAULT_HARDLIMIT;
        ret = prctl(PR_WGCM_CTL, WGCM_CTL_SET_GRP, &grp, 0, 0);
        if (ret) {
            FFRT_LOGE("[SERVER] wgcm group %u register failed\n ret is %d", i, ret);
        }
    }

    while (true) {
        struct wgcm_workergrp_data data = {0, 0, 0, 0, 0, 0, 0, 0};
        ret = prctl(PR_WGCM_CTL, WGCM_CTL_WAIT, &data, 0, 0);
        if (ret) {
            FFRT_LOGE("[SERVER] wgcm server wait failed ret is %d", ret);
            sleep(1);
            continue;
        }
        if (data.woken_flag == WGCM_ACTIVELY_WAKE) {
            break;
        }

        for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
            monitor->HandleBlocked(QoS(qos));
        }
    }
    ret = prctl(PR_WGCM_CTL, WGCM_CTL_UNREGISTER, 0, 0, 0);
    if (ret) {
        FFRT_LOGE("[SERVER] wgcm server unregister failed ret is %d.", ret);
    }
}

void CPUMonitor::SetupMonitor()
{
    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        ctrlQueue[qos].hardLimit = DEFAULT_HARDLIMIT;
        ctrlQueue[qos].workerManagerID = static_cast<uint32_t>(qos);
        ctrlQueue[qos].maxConcurrency = GlobalConfig::Instance().getCpuWorkerNum(QoS(qos));
    }
}

int CPUMonitor::SetWorkerMaxNum(const QoS& qos, int num)
{
    WorkerCtrl& workerCtrl = ctrlQueue[qos()];
    workerCtrl.lock.lock();
    static bool setFlag[QoS::MaxNum()] = {false};
    if (setFlag[qos()]) {
        FFRT_LOGE("qos[%d] worker num can only been setup once", qos());
        workerCtrl.lock.unlock();
        return -1;
    }
    if (num <= 0 || num > QOS_WORKER_MAXNUM) {
        FFRT_LOGE("qos[%d] worker num[%d] is invalid.", qos(), num);
        workerCtrl.lock.unlock();
        return -1;
    }
    workerCtrl.maxConcurrency = num;
    setFlag[qos()] = true;
    workerCtrl.lock.unlock();
    return 0;
}

void CPUMonitor::RegWorker(const QoS& qos)
{
    struct wgcm_workergrp_data grp = {0, 0, 0, 0, 0, 0, 0, 0};
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
    struct wgcm_workergrp_data grp = {0, 0, 0, 0, 0, 0, 0, 0};
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

void CPUMonitor::TimeoutCount(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.lock.unlock();
}

void CPUMonitor::WakeupCount(const QoS& qos, bool isDeepSleepWork)
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

void CPUMonitor::IntoDeepSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.deepSleepingWorkerNum++;
    workerCtrl.lock.unlock();
}

void CPUMonitor::OutOfDeepSleep(const QoS& qos)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    workerCtrl.sleepingWorkerNum--;
    workerCtrl.executionNum++;
    workerCtrl.deepSleepingWorkerNum--;
    workerCtrl.lock.unlock();
}

#ifdef FFRT_IO_TASK_SCHEDULER
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
#endif

bool CPUMonitor::IsExceedDeepSleepThreshold()
{
    int totalWorker = 0;
    int deepSleepingWorkerNum = 0;
    for (unsigned int i = 0; i < static_cast<unsigned int>(QoS::Max()); i++) {
        WorkerCtrl& workerCtrl = ctrlQueue[i];
        workerCtrl.lock.lock();
        deepSleepingWorkerNum += workerCtrl.deepSleepingWorkerNum;
        totalWorker += workerCtrl.executionNum + workerCtrl.sleepingWorkerNum;
        workerCtrl.lock.unlock();
    }
    return deepSleepingWorkerNum * 2 > totalWorker;
}

void CPUMonitor::Poke(const QoS& qos, TaskNotifyType notifyType)
{
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();

#ifdef FFRT_IO_TASK_SCHEDULER
    bool tiggerSuppression = (ops.GetWorkerCount(qos) > TIGGER_SUPPRESS_WORKER_COUNT) &&
        (workerCtrl.executionNum > TIGGER_SUPPRESS_EXECUTION_NUM) && (ops.GetTaskCount(qos) < workerCtrl.executionNum);
    if (notifyType != TaskNotifyType::TASK_ADDED && tiggerSuppression) {
        workerCtrl.lock.unlock();
        return;
    }
#endif

    FFRT_LOGD("qos[%d] exe num[%d] slp num[%d]", (int)qos, workerCtrl.executionNum, workerCtrl.sleepingWorkerNum);
    if (static_cast<uint32_t>(workerCtrl.executionNum - ops.GetBlockingNum(qos)) < workerCtrl.maxConcurrency) {
        if (workerCtrl.sleepingWorkerNum == 0) {
            if (static_cast<uint32_t>(workerCtrl.executionNum) < workerCtrl.hardLimit) {
                workerCtrl.executionNum++;
                workerCtrl.lock.unlock();
                ops.IncWorker(qos);
            } else {
                workerCtrl.lock.unlock();
            }
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

bool CPUMonitor::IsExceedMaxConcurrency(const QoS& qos)
{
    bool ret = false;
    WorkerCtrl& workerCtrl = ctrlQueue[static_cast<int>(qos)];
    workerCtrl.lock.lock();
    if (static_cast<uint32_t>(workerCtrl.executionNum - ops.GetBlockingNum(qos)) > workerCtrl.maxConcurrency) {
        ret = true;
    }
    workerCtrl.lock.unlock();
    return ret;
}

}
