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
#include "rtg_interface.h"
#include "concurrent_task_client.h"
#include "dfx/log/ffrt_log_api.h"

constexpr int HWC_UID = 3039;
constexpr int ROOT_UID = 0;
constexpr int SYSTEM_UID = 1000;
constexpr int RS_RTG_ID = 10;

using namespace OHOS::ConcurrentTask;

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
    int addRet = OHOS::RME::AddThreadToRtg(tid, wgId);
    if (addRet == 0) {
        FFRT_LOGI("[WorkGroup] update thread %d success", tid);
    } else {
        FFRT_LOGE("[WorkGroup] update thread %d failed, return %d", tid, addRet);
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

    if (OHOS::RME::BeginFrameFreq(wg->rtgId, 0) == 0) {
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

    int ret = OHOS::RME::EndFrameFreq(wg->rtgId);
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
    int rtgId = -1;
    int uid = getuid();
    int num = 0;

    if (uid == SYSTEM_UID || uid == HWC_UID) {
        ConcurrentTaskClient::GetInstance().QueryInterval(QUERY_RENDER_SERVICE, rs);
        rtgId = rs.rtgId;
    } else if (uid == ROOT_UID) {
        rtgId = OHOS::RME::CreateNewRtgGrp(num);
    } else {
        ConcurrentTaskClient::GetInstance().QueryInterval(QUERY_UI, rs);
        rtgId = rs.rtgId;
    }

    if (rtgId < 0) {
        FFRT_LOGE("[WorkGroup] create rtg group %d failed", rtgId);
        return nullptr;
    }
    FFRT_LOGI("[WorkGroup] create rtg group %d success", rtgId);

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
    FFRT_LOGI("[WorkGroup] %s uid = %d rtgid = %d", __func__, (int)getuid(), wg->rtgId);
    int addRet = OHOS::RME::AddThreadToRtg(tid, wg->rtgId);
    if (addRet != 0) {
        FFRT_LOGE("[WorkGroup] join fail with %d threads for %d", addRet, tid);
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
    if (uid != SYSTEM_UID && uid != HWC_UID) {
        ret = OHOS::RME::DestroyRtgGrp(wg->rtgId);
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
