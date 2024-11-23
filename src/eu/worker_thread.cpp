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

#include "eu/worker_thread.h"
#include <algorithm>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include "dfx/log/ffrt_log_api.h"
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
#include "eu/blockaware.h"
#endif
#include "eu/execute_unit.h"
#include "eu/osattr_manager.h"
#include "eu/qos_interface.h"
#include "internal_inc/osal.h"
#include "qos.h"
#include "util/ffrt_facade.h"
#include "util/name_manager.h"

namespace ffrt {
constexpr int MAX_RT_PRIO = 89;
constexpr int MAX_VIP_PRIO = 99;
constexpr int MAX_CFS_PRIO = 100;
constexpr int DEFAULT_PRIO = 120;
WorkerThread::WorkerThread(const QoS& qos) : exited(false), idle(false), tid(-1), qos(qos)
{
#ifdef FFRT_PTHREAD_ENABLE
    pthread_attr_init(&attr_);
    size_t stackSize = FFRTFacade::GetEUInstance().GetGroupCtl()[qos()].workerStackSize;
    if (stackSize > 0) {
        pthread_attr_setstacksize(&attr_, stackSize);
    }
#endif
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    domain_id = (qos() <= BLOCKAWARE_DOMAIN_ID_MAX) ? qos() : BLOCKAWARE_DOMAIN_ID_MAX + 1;
#endif
}

void WorkerThread::NativeConfig()
{
    pid_t pid = syscall(SYS_gettid);
    this->tid = pid;
    SetThreadAttr(this, qos);
}

void WorkerThread::WorkerSetup(WorkerThread* wthread)
{
    static int threadIndex[QoS::MaxNum()] = {0};
    std::string qosStr = std::to_string(qos());
    std::string threadName = std::string(WORKER_THREAD_NAME_PREFIX) + qosStr +
        std::string(WORKER_THREAD_SYMBOL) + std::to_string(threadIndex[qos()]++);
    if (qosStr == "") {
        FFRT_LOGE("ffrt threadName qos[%d] index[%d]", qos(), threadIndex[qos()]);
    }
    pthread_setname_np(wthread->GetThread(), threadName.c_str());
}

int SetCpuAffinity(unsigned long affinity, int tid)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for (unsigned long i = 0; i < sizeof(affinity) * 8; i++) {
        if ((affinity & (static_cast<unsigned long>(1) << i)) != 0) {
            CPU_SET(i, &mask);
        }
    }
    int ret = syscall(__NR_sched_setaffinity, tid, sizeof(mask), &mask);
    if (ret < 0) {
        FFRT_LOGE("set qos affinity failed for tid %d\n", tid);
    }
    return ret;
}

void SetThreadPriority(unsigned char priority, WorkerThread* thread)
{
    int ret = 0;
    if (priority < MAX_RT_PRIO) {
        struct sched_param param;
        param.sched_priority = MAX_RT_PRIO - priority;
        ret = pthread_setschedparam(thread->GetThread(), SCHED_RR, &param);
        if (ret != 0) {
            FFRT_LOGE("[%d] set priority failed ret[%d] errno[%d]", thread->Id(), ret, errno);
        }
    } else if (priority < MAX_VIP_PRIO) {
        pid_t pid = getpid();
        const std::string path = "/proc/" + std::to_string(pid) + "/task/" + std::to_string(thread->Id()) + "/vip_prio";
        int vip_prio = MAX_VIP_PRIO - priority;
        OSAttrManager::Instance->SetCGroupPara(path, vip_prio);
    } else {
        struct sched_param param;
        param.sched_priority = 0;
        ret = pthread_setschedparam(thread->GetThread(), SCHED_OTHER, &param);
        if (ret != 0) {
            FFRT_LOGE("[%d] set priority sched_normal failed ret[%d] errno[%d]", thread->Id(), ret, errno);
        }
        ret = setpriority(PRIO_PROCESS, thread->Id(), priority - DEFAULT_PRIO);
        if (ret != 0) {
            FFRT_LOGE("[%d] set priority failed ret[%d] errno[%d]", thread->Id(), ret, errno);
        }
    }
}

// void SetThreadAttr(WorkerThread* thread, const QoS& qos)
// {
//     constexpr int processNameLen = 32;
//     static std::once_flag flag;
//     static char processName[processNameLen];
//     std::call_once(flag, []() {
//         GetProcessName(processName, processNameLen);
//     });
//     if (strstr(processName, "CameraDaemon")) {
//         SetCameraThreadAttr(thread, qos);
//     } else {
//         SetDefaultThreadAttr(thread, qos);
//     }
// }

void SetDefaultThreadAttr(WorkerThread* thread, const QoS& qos)
{
    if (qos() <= qos_max) {
        FFRTQosApplyForOther(qos(), thread->Id());
        FFRT_LOGD("qos apply tid[%d] level[%d]", thread->Id(), qos());
    } else {
        FFRT_LOGE("default qos:%d is invalid", qos());
    }
}
}; // namespace ffrt
