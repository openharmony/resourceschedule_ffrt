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

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include "qos_interface.h"
#include "dfx/log/ffrt_log_api.h"
#include "qos_config.h"
#include "qos_policy.h"

namespace ffrt {
int SetQosPolicy(struct QosPolicyDatas *policyDatas)
{
    return QosPolicy(policyDatas);
}

static __attribute__((constructor)) void QosPolicyInit()
{
    int ret;

    ret = SetQosPolicy(&QosConfig::Instance().getPolicyDefault());
    if (ret) {
        FFRT_LOGE("uid %d set g_defaultQosPolicy failed", getuid());
    }

    ret = SetQosPolicy(&QosConfig::Instance().getPolicyForeground());
    if (ret) {
        FFRT_LOGE("uid %d set g_foregroundQosPolicy failed", getuid());
    }

    ret = SetQosPolicy(&QosConfig::Instance().getPolicyBackground());
    if (ret) {
        FFRT_LOGE("uid %d set g_backgroundQosPolicy failed", getuid());
    }

    ret = SetQosPolicy(&QosConfig::Instance().getPolicySystem());
    if (ret) {
        FFRT_LOGE("uid %d set g_systemServerQosPolicy failed", getuid());
    }

    setFuncAffinity(SetAffinity);
    setFuncPriority(SetPriority);
    FFRT_LOGI("set qos policy finish");
}

int SetAffinity(unsigned long affinity, int tid)
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

int SetPriority(unsigned char priority, WorkerThread* thread)
{
    int ret;
    if (priority < MAX_USER_RT_PRIO) {
        struct sched_param param;
        param.sched_priority = MAX_USER_RT_PRIO - priority;
        ret = pthread_setschedparam(thread->GetThread().native_handle(), SCHED_RR, &param);
        if (ret != 0) {
            FFRT_LOGE("[%d] set priority failed errno[%d]\n", thread->Id(), errno);
        }
    } else {
        ret = setpriority(PRIO_PROCESS, thread->Id(), priority - DEFAULT_PRIO);
        if (ret != 0) {
            FFRT_LOGE("[%d] set priority failed errno[%d]\n", thread->Id(), errno);
        }
    }
    return ret;
}
}
