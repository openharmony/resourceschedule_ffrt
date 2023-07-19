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

#include "worker_thread.h"

#include <algorithm>
#include <unistd.h>

#include <sys/syscall.h>
#include "dfx/log/ffrt_log_api.h"
#include "eu/osattr_manager.h"
#include "eu/qos_config.h"
#include "internal_inc/config.h"
namespace ffrt {
void WorkerThread::NativeConfig()
{
    pid_t pid = syscall(SYS_gettid);
    this->tid = pid;
}

void WorkerThread::WorkerSetup(WorkerThread* wthread, const QoS& qos)
{
    pthread_setname_np(wthread->GetThread().native_handle(), ("ffrtwk/CPU-" + (std::to_string(qos()))+ "-" +
        std::to_string(GlobalConfig::Instance().getQosWorkers()[static_cast<int>(qos())].size())).c_str());
    GlobalConfig::Instance().setQosWorkers(qos, wthread->Id());
    if (qos() != qos_max) {
        QosApplyForOther(qos(), wthread->Id());
        FFRT_LOGD("qos apply tid[%d] level[%d]\n", wthread->Id(), qos());
        if (getFuncAffinity() != nullptr) {
            getFuncAffinity()(QosConfig::Instance().getPolicySystem().policys[qos()].affinity, wthread->Id());
        }
        if (getFuncPriority() != nullptr) {
            getFuncPriority()(QosConfig::Instance().getPolicySystem().policys[qos()].priority, wthread);
        }
    } else {
        OSAttrManager::Instance()->SetTidToCGroup(wthread->Id());
    }
}
}; // namespace ffrt
