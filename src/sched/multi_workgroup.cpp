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

#define MIN_FRAME_BUFFER 2
#define MIN_LOAD_LEVEL 20
#define MAX_LOAD_LEVEL 64
#define MAX_LOAD_UTIL 736
#define DEFAUT_FRAME_NUM 3
#define RAPID_UP_MARGIN (-1600)
#define RAPID_UP_CALM_TIEMR 2
#define RAPIDLY_MARGIN_UP_STEP 4
#define MARGIN_UP_STEP 2
#define MARGIN_DOWN_STEP 1
#define RESERVE_MARGIN_US 500
#define KEEP_MARGIN_US 3000

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
        FFRT_LOGI("[WorkGroup] update thread %{public}d success", tid);
    } else {
        FFRT_LOGE("[WorkGroup] update thread %{public}d failed, return %{public}d", tid, addRet);
    }
    return true;
}

static void InitLoadData(struct LoadData* ld, int frameNum)
{
    if (ld == nullptr) {
        FFRT_LOGE("Error LoadData object");
        return;
    }
    if (frameNum > MAX_FRAME_BUFFER) {
        FFRT_LOGW("LoadData frameNum set too big, force to %d", MAX_FRAME_BUFFER);
        ld->frameNum = MAX_FRAME_BUFFER;
    } else if (frameNum <= MIN_FRAME_BUFFER) {
        ld->frameNum = MIN_FRAME_BUFFER;
    } else {
        ld->frameNum = frameNum;
    }
    for (int i = 0; i < ld->frameNum; i++) {
        ld->frameLen[i] = 0;
    }
    ld->curIndex = 0;
    ld->curLoadLevel = MIN_LOAD_LEVEL;
    ld->paddingStatus = true;
    ld->avgFrameLen = 0;
    ld->sumFrameLen = 0;
    ld->calmTimer = 0;
}

static void UpdateLoadStartTime(struct LoadData* ld)
{
    if (ld == nullptr) {
        FFRT_LOGE("Error LoadData object");
        return;
    }
    ld->lastStartTime = std::chrono::steady_clock::now();
}

static void UpdateLoadEndTime(struct LoadData* ld)
{
    if (ld == nullptr) {
        FFRT_LOGE("Error LoadData object");
        return;
    }
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    ld->lastEndTime = now;
    std::chrono::duration<double, std::micro> us = now - ld->lastStartTime;
    int frameLen = us.count();
    int oldestLen = ld->frameLen[ld->curIndex];
    ld->frameLen[ld->curIndex] = frameLen;
    if (ld->paddingStatus) {
        ld->sumFrameLen += frameLen;
        ld->avgFrameLen = ld->sumFrameLen / (ld->curIndex + 1);
        ld->curIndex++;
        if (ld->curIndex >= ld->frameNum) {
            ld->paddingStatus = false;
            ld->curIndex = 0;
        }
        return;
    }
    ld->sumFrameLen = ld->sumFrameLen + (frameLen - oldestLen);
    ld->avgFrameLen = ld->sumFrameLen / ld->frameNum;
    ld->curIndex = (ld->curIndex + 1) % ld->frameNum;
}

static int CalculateEndMargin(struct LoadData* ld, uint64_t deadline)
{
    if (ld == nullptr) {
        FFRT_LOGE("Error LoadData object");
        return 0;
    }
    int lastFrameLen = ld->frameLen[(ld->curIndex - 1) % ld->frameNum];
    if (lastFrameLen > deadline + RESERVE_MARGIN_US) { // if drop frame, need boost
        if (ld->curLoadLevel < MAX_LOAD_LEVEL) {
            ld->curLoadLevel += MARGIN_UP_STEP;
        }
        return MAX_LOAD_LEVEL;
    }
    return 0;
}

static int CalculateStartMargin(struct LoadData* ld, uint64_t deadline)
{
    if (ld == nullptr) {
        FFRT_LOGE("Error LoadData object");
        return 0;
    }
    if (ld->avgFrameLen > deadline) {
        return MAX_LOAD_LEVEL;
    }
    if ((ld->avgFrameLen > deadline - RAPID_UP_MARGIN) && !ld->calmTimer) {
        ld->curLoadLevel += RAPIDLY_MARGIN_UP_STEP;
        ld->calmTimer = RAPID_UP_CALM_TIEMR;
    } else if (ld->avgFrameLen > deadline - RESERVE_MARGIN_US) {
        ld->curLoadLevel += MARGIN_UP_STEP;
    } else if (ld->avgFrameLen <= deadline - RESERVE_MARGIN_US - KEEP_MARGIN_US) {
        ld->curLoadLevel -= MARGIN_DOWN_STEP;
    }
    if (ld->curLoadLevel > MAX_LOAD_LEVEL) {
        ld->curLoadLevel = MAX_LOAD_LEVEL;
    }
    if (ld->curLoadLevel < MIN_LOAD_LEVEL) {
        ld->curLoadLevel = MIN_LOAD_LEVEL;
    }
    if (!ld->calmTimer) {
        ld->calmTimer--;
    }
    return ld->curLoadLevel;
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

    UpdateLoadStartTime(&wg->ld);
    int level = CalculateStartMargin(&wg->ld, wg->interval);
    if (OHOS::RME::BeginFrameFreq(wg->rtgId, 0) == 0) {
        if (wg->rtgId == RS_RTG_ID) {
            OHOS::RME::SetMinUtil(wg->rtgId, MAX_LOAD_UTIL / MAX_LOAD_LEVEL * level);
        }
        wg->started = true;
    } else {
        FFRT_LOGE("[WorkGroup] start rtg(%{public}d) work interval failed", wg->rtgId);
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

    UpdateLoadEndTime(&wg->ld);
    CalculateEndMargin(&wg->ld, wg->interval);
    int ret = OHOS::RME::EndFrameFreq(wg->rtgId);
    if (wg->rtgId == RS_RTG_ID) {
        OHOS::RME::SetMinUtil(wg->rtgId, 0);
    }
    if (ret == 0) {
        wg->started = false;
    } else {
        FFRT_LOGE("[WorkGroup] stop rtg(%{public}d) work interval failed", wg->rtgId);
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

    InitLoadData(&wg->ld, DEFAUT_FRAME_NUM);
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
        FFRT_LOGE("[WorkGroup] create rtg group %{public}d failed", rtgId);
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
    FFRT_LOGI("[WorkGroup] %{public}s uid = %{public}d rtgid = %{public}d", __func__, (int)getuid(), wg->rtgId);
    int addRet = OHOS::RME::AddThreadToRtg(tid, wg->rtgId);
    if (addRet == 0) {
        FFRT_LOGI("[WorkGroup] join thread %{public}ld success", tid);
    } else {
        FFRT_LOGE("[WorkGroup] join fail with %{public}d threads for %{public}d", addRet, tid);
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
            FFRT_LOGI("[WorkGroup] destroy rtg group success");
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
