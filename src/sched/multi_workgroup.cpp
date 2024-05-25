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

#ifdef QOS_FRAME_RTG
#include "workgroup_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <mutex>
#include "dfx/log/ffrt_log_api.h"
#include "task_client_adapter.h"

constexpr int HWC_UID = 3039;
constexpr int ROOT_UID = 0;
constexpr int RS_UID = 1003;
constexpr int RS_RTG_ID = 10;

namespace ffrt {
static int wgId = -1;
static int wgCount = 0;
static std::mutex wgLock;

bool JoinWG(int tid)
{
    if (wgId < 0) {
        if (wgCount > 0) {
            FFRT_LOGE("[WorkGroup] interval is unavailable");
        }
        return false;
    }
    IntervalReply rs;
    rs.rtgId = -1;
    rs.tid = tid;
    int uid = getuid();
    if (uid == RS_UID) {
        _CTC_QueryInterval(QUERY_RENDER_SERVICE, rs);
        if (rs.rtgId > 0) {
            FFRT_LOGE("[WorkGroup] update thread %{public}d success", tid);
        } else {
            FFRT_LOGE("[WorkGroup] update thread %{public}d failed", tid);
        }
    } else {
        int addRet = _AddThreadToRtg(tid, wgId, 0);
        if (addRet == 0) {
            FFRT_LOGE("[WorkGroup] update thread %{public}d success", tid);
        } else {
            FFRT_LOGE("[WorkGroup] update thread %{public}d failed, return %{public}d", tid, addRet);
        }
    }
    return true;
}

void WorkgroupStartInterval(struct Workgroup* wg)
{
    if (wg == nullptr) {
        FFRT_LOGE("[WorkGroup] input workgroup is null");
        return;
    }

    if (wg->started) {
        FFRT_LOGW("[WorkGroup] already start");
        return;
    }

    if (_BeginFrameFreq(0) == 0) {
        wg->started = true;
    } else {
        FFRT_LOGE("[WorkGroup] start rtg(%d) work interval failed", wg->rtgId);
    }
}

void WorkgroupStopInterval(struct Workgroup* wg)
{
    if (wg == nullptr) {
        FFRT_LOGE("[WorkGroup] input workgroup is null");
        return;
    }

    if (!wg->started) {
        FFRT_LOGW("[WorkGroup] already stop");
        return;
    }

    int ret = _EndFrameFreq(0);
    if (ret == 0) {
        wg->started = false;
    } else {
        FFRT_LOGE("[WorkGroup] stop rtg(%d) work interval failed", wg->rtgId);
    }
}

static void WorkgroupInit(struct Workgroup* wg, uint64_t interval, int rtgId)
{
    wg->started = false;
    wg->interval = interval;
    wg->rtgId = rtgId;
    wgId = rtgId;

    for (int i = 0; i < MAX_WG_THREADS; i++) {
        wg->tids[i] = -1;
    }
}

struct Workgroup* WorkgroupCreate(uint64_t interval)
{
    IntervalReply rs;
    rs.rtgId = -1;
    rs.tid = -1;
    int rtgId = -1;
    int uid = getuid();
    int num = 0;

    if (uid == RS_UID) {
        _CTC_QueryInterval(QUERY_RENDER_SERVICE, rs);
        rtgId = rs.rtgId;
        FFRT_LOGI("[WorkGroup] query render_service %{public}d, %{public}d", rtgId, uid);
    }

    if (rtgId < 0) {
        FFRT_LOGE("[WorkGroup] create rtg group %d failed", rtgId);
        return nullptr;
    }
    FFRT_LOGI("[WorkGroup] create rtg group %{public}d success", rtgId);

    Workgroup* wg = nullptr;
    wg = new struct Workgroup();
    if (wg == nullptr) {
        FFRT_LOGE("[WorkGroup] workgroup malloc failed!");
        return nullptr;
    }
    WorkgroupInit(wg, interval, rtgId);
    {
        std::lock_guard<std::mutex> lck(wgLock);
        wgCount++;
    }
    return wg;
}

void WorkgroupJoin(struct Workgroup* wg, int tid)
{
    if (wg == nullptr) {
        FFRT_LOGE("[WorkGroup] input workgroup is null");
        return;
    }
    int uid = getuid();
    FFRT_LOGI("[WorkGroup] %s uid = %d rtgid = %d", __func__, uid, wg->rtgId);
    if (uid == RS_UID) {
        IntervalReply rs;
        rs.tid = tid;
        _CTC_QueryInterval(QUERY_RENDER_SERVICE, rs);
        FFRT_LOGI("[WorkGroup] join thread %{public}ld", tid);
    } else {
        int addRet = _AddThreadToRtg(tid, wg->rtgId, 0);
        if (addRet == 0) {
            FFRT_LOGI("[WorkGroup] join thread %{public}ld success", tid);
        } else {
            FFRT_LOGE("[WorkGroup] join fail with %{public}d threads for %{public}d", addRet, tid);
        }
    }
}

int WorkgroupClear(struct Workgroup* wg)
{
    if (wg == nullptr) {
        FFRT_LOGE("[WorkGroup] input workgroup is null");
        return 0;
    }
    int ret = -1;
    int uid = getuid();
    if (uid != RS_UID) {
        ret = _DestroyRtgGrp(wg->rtgId);
        if (ret != 0) {
            FFRT_LOGE("[WorkGroup] destroy rtg group failed");
        } else {
            {
                std::lock_guard<std::mutex> lck(wgLock);
                wgCount--;
            }
        }
    }
    delete wg;
    wg = nullptr;
    return ret;
}
}

#endif /* QOS_FRAME_RTG */
