#include "c/timer.h"
#include "sync/poller.h"
#include "internal_inc/osal.h"
#include "dfx/log/ffrt_log_api.h"
#include "util/ffrt_facade.h"

#ifdef FFRT_IO_TASK_SCHEDULER
API_ATTRIBUTE((visibility("default")))
ffrt_timer_t ffrt_time_start(ffrt_qos_t qos, uint64_t timeout, void* data, ffrt_timer_cb cb, bool repeat)
{
    ffrt::QoS pollerQos = ffrt::QoS(qos);
    int handle = ffrt::PollerProxy::Instance()->GetPoller(pollerQos).RegisterTimer(timeout, data, cb);
    if (handle >= 0) {
        ffrt::FFRTFacade::GetEUInstance().NotifyLocalTaskAdded(pollerQos);
    }
    return handle;
}

API_ATTRIBUTE((visibility("default")))
int ffrt_timer_stop(ffrt_qos_t qos, int handle)
{
    return ffrt::PollerProxy::Instance()->GetPoller(ffrt::QoS(qos)).UnregisterTimer(handle);
}

API_ATTRIBUTE((visibility("default")))
ffrt_timer_query_t ffrt_timer_query(ffrt_qos_t qos, int handle)
{
    return ffrt:: PollerProxy::Instance()->GetPoller(ffrt::QoS(qos)).GetTimerStatus(handle);
}
#endif