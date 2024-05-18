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

#include "c/timer.h"
#include "sync/poller.h"
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
ffrt_timer_t ffrt_timer_start(ffrt_qos_t qos, uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat)
{
    ffrt::QoS pollerQos;
    if (!QosConvert(qos, pollerQos)) {
        return -1;
    }

    if (cb == nullptr) {
        FFRT_LOGE("cb cannot be null");
        return -1;
    }

    int handle = ffrt::PollerProxy::Instance()->GetPoller(pollerQos).RegisterTimer(timeout, data, cb, repeat);
    if (handle >= 0) {
        ffrt::FFRTFacade::GetEUInstance().NotifyLocalTaskAdded(pollerQos);
    }
    return handle;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_timer_stop(ffrt_qos_t qos, int handle)
{
    ffrt::QoS pollerQos;
    if (!QosConvert(qos, pollerQos)) {
        return -1;
    }
    return ffrt::PollerProxy::Instance()->GetPoller(pollerQos).UnregisterTimer(handle);
}

API_ATTRIBUTE((visibility("default")))
ffrt_timer_query_t ffrt_timer_query(ffrt_qos_t qos, int handle)
{
    ffrt::QoS pollerQos;
    if (!QosConvert(qos, pollerQos)) {
        return ffrt_timer_notfound;
    }
    return ffrt::PollerProxy::Instance()->GetPoller(pollerQos).GetTimerStatus(handle);
}
#endif