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

#include "c/executor_task.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/ffrt_facade.h"

#ifdef FFRT_IO_TASK_SCHEDULER
static bool QosConvert(ffrt_qos_t qos, ffrt::QoS& mappedQos)
{
    if (ffrt::GetFuncQosMap() == nullptr) {
        FFRT_LOGE("FuncQosMap has not regist");
        return false;
    }
    mappedQos = ffrt::QoS(ffrt::GetFuncQosMap()(qos));
    if (mappedQos == ffrt::qos_inherit) {
        mappedQos = ffrt::ExecuteCtx::Cur()->qos();
    }
    return true;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_epoll_ctl(ffrt_qos_t qos, int op, int fd, uint32_t events, void* data, ffrt_poller_cb cb)
{
    ffrt::QoS ffrtQos;
    if (!QosConvert(qos, ffrtQos)) {
        return -1;
    }
    if (op == EPOLL_CTL_DEL) {
        return ffrt::PollerProxy::Instance()->GetPoller(ffrtQos).DelFdEvent(fd);
    } else if (op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) {
        int ret = ffrt::PollerProxy::Instance()->GetPoller(ffrtQos).AddFdEvent(op, events, fd, data, cb);
        if (ret == 0) {
            ffrt::FFRTFacade::GetEUInstance().NotifyLocalTaskAdded(ffrtQos);
        }
        return ret;
    } else {
        FFRT_LOGE("ffrt_epoll_ctl input error: op=%d, fd=%d", op, fd);
        return -1;
    }
}

API_ATTRIBUTE((visibility("default")))
int ffrt_epoll_wait(ffrt_qos_t qos, struct epoll_event* events, int max_events, int timeout)
{
    ffrt::QoS ffrtQos;
    if (!QosConvert(qos, ffrtQos)) {
        return -1;
    }
    return ffrt::PollerProxy::Instance()->GetPoller(ffrtQos).WaitFdEvent(events, max_events, timeout);
}

API_ATTRIBUTE((visibility("default")))
void ffrt_poller_wakeup(ffrt_qos_t qos)
{
    ffrt::QoS pollerQos;
    if (!QosConvert(qos, pollerQos)) {
        return;
    }

    ffrt::PollerProxy::Instance()->GetPoller(pollerQos).WakeUp();
}

API_ATTRIBUTE((visibility("default")))
uint8_t ffrt_epoll_get_count(ffrt_qos_t qos)
{
    ffrt::QoS pollerQos;
    if (!QosConvert(qos, pollerQos)) {
        return 0;
    }

    return ffrt::PollerProxy::Instance()->GetPoller(pollerQos).GetPollCount();
}

API_ATTRIBUTE((visibility("default")))
uint64_t ffrt_epoll_get_wait_time(void* taskHandle)
{
    if (taskHandle == nullptr) {
        FFRT_LOGE("invalid task handle");
        return false;
    }

    auto task = reinterpret_cast<ffrt::CPUEUTask*>(taskHandle);
    return ffrt::PollerProxy::Instance()->GetPoller(task->qos).GetTaskWaitTime(task);
}
#endif