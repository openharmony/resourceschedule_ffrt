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
#include <sys/syscall.h>
#include "dfx/log/ffrt_log_api.h"
#include "eu/qos_interface.h"
#include "qos.h"
namespace ffrt {
void WorkerThread::NativeConfig()
{
    pid_t pid = syscall(SYS_gettid);
    this->tid = pid;
}

void WorkerThread::WorkerSetup(WorkerThread* wthread)
{
    static int threadIndex[QoS::Max()] = {0};
    std::string threadName = "ffrtwk/CPU-" + (std::to_string(qos()))+ "-" + std::to_string(threadIndex[qos()]++);
    if (std::to_string(qos()) == "") {
        FFRT_LOGE("ffrt threadName qos[%d] index[%d]", qos(), threadIndex[qos()]);
    }
    pthread_setname_np(wthread->GetThread().native_handle(), threadName.c_str());
    SetThreadAttr(wthread, qos);
}

void SetThreadAttr(WorkerThread* thread, const QoS& qos)
{
    if (qos() <= qos_max) {
        QosApplyForOther(qos(), thread->Id());
        FFRT_LOGD("qos apply tid[%d] level[%d]\n", thread->Id(), qos());
    }
}
}; // namespace ffrt
