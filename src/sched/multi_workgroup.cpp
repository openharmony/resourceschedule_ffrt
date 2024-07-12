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


#include "workgroup_internal.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include "dfx/log/ffrt_log_api.h"
#include "task_client_adapter.h"

#if (defined(QOS_WORKER_FRAME_RTG) || defined(QOS_FRAME_RTG))
constexpr int HWC_UID = 3039;
constexpr int ROOT_UID = 0;
constexpr int RS_RTG_ID = 10;

namespace ffrt {
static int wgId = -1;
static Workgroup rsWorkGroup = nullptr;
static int wgCount = 0;
static std::mutex wgLock;

#if defined(QOS_WORKER_FRAME_RTG)

void WorkgroupInit(struct Workgroup* wg, uint64_t interval, int rtgId)
{
    wg->started = false;
    wg->interval = interval;
    wg->rtgId = rtgId;
    wgId = rtgId;

    for (int i = 0; i < MAX_WG_THREADS; i++) {
        wg->tids[i] = -1;
    }
}

int FindThreadInWorkGroup(Workgroup *workGroup, int tid)
{
    if (workGroup != nullptr) {
        return -1;
    }
    for (int i = 0;i < MAX_WG_THREADS; i++) {
        if (workGroup->tids[i] == tid) {
           return i;
        }
    }
    return 1;
}

bool InsertThreadInWorkGroup(Workgroup *workGroup, int tid){
    if (workGroup != nullptr) {
        return -1;
    }
    int targetIndex = -1;
    for (int i = 0;i < MAX_WG_THREADS; i++) {
        if (workGroup->tids[i] == -1) {
           workGroup->tids[i] = tid;
           targetIndex = i;
           break;
        }
    }
    if (targetIndex == -1) {
        return false;
    }
    return true;
}

void CreateRSWorkGroup(uint64_t interval) {
    IntervalReply rs;
    rs.rtgId = -1;
    rs.tid = -1;
    {
        std::lock_guard<std::mutex> lck(wgLock);
        if (rsWorkGroup == nullptr) {
             CTC_QUERY_INTERVAL(QUERY_RENDER_SERVICE, td);
             if(rs.rtgId > 0) {
                rsWorkGroup = new struct Workgroup();
                if (rsWorkGroup == nullptr) {
                    return;
                }
                WorkgroupInit(rsWorkGroup, interval, rs.rtgId);
                wgCount++;
             }
        }
    }
}

void LeaveRSWorkGroup(int tid) {
    std::lock_guard<std::mutex> lck(wgLock);
    if (rsWorkGroup == nullptr) {
        return false;
    }
    int exitIndex = FindThreadInWorkGroup(rsWorkGroup, tid);
    if (existIndex != -1) {
        workGroup->tids[i] = -1;
    }
    return true;
}

void JoinRSWorkGroup(int tid) {
    std::lock_guard<std::mutex> lck(wgLock);
    if (rsWorkGroup == nullptr) {
        return false;
    }
    int exitIndex = FindThreadInWorkGroup(rsWorkGroup, tid);
    if (existIndex == -1) {
        IntervalReply rs;
        rs.rtgId = -1;
        rs.tid = tid;
        CTC_QUERY_INTERVAL(QUERY_RENDER_SERVICE, td);
        if(rs.rtgId > 0) {
            bool success = InsertThreadInWorkGroup(rsWorkGroup, tid);
            if (!success) {
                return false;
            }
        }
    }
    return true;
}

bool DestoryRSWorkGroup() {
    std::lock_guard<std::mutex> lck(wgLock);
    if (rsWorkGroup != nullptr) {
        delete rsWorkGroup;
        rsWorkGroup = nullptr;
        wgId = -1;
        return true;
    }
    return false;
}



bool JoinWG(int tid)
{
    if (wgId < 0) {
        if (wgCount > 0) {
            FFRT_LOGE("[WorkGroup] interval is unavailable");
        }
        return false;
    }

    int uid = getuid();
    if (uid == RS_UID) {
        return JoinRSWorkGroup(tid);
    }
    int addRet = AddThreadToRtgAdapter(tid, wgId, 0);
    if (addRet == 0) {
        FFRT_LOGI("[WorkGroup] update thread %{public}d success", tid);
    } else {
        FFRT_LOGE("[WorkGroup] update thread %{public}d failed, return %{public}d", tid, addRet);
    }
    return true;
}

bool LeaveWG(int tid) {
    int uid = getuid();
    if (uid == RS_UID) {
        return LeaveRSWorkGroup(tid);
    }
    return false;
}

struct Workgroup* WorkgroupCreate(uint64_t interval)
{
    int uid = getuid();
    if (uid == RS_UID) {
        CreateRSWorkGroup(interval);
        return rsWorkGroup;
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

int WorkgroupClear(struct Workgroup* wg)
{
    int uid = getuid();
    if (uid == RS_UID) {
        return DestoryRSWorkGroup();
    }

    if (wg == nullptr) {
        FFRT_LOGE("[WorkGroup] input workgroup is null");
        return 0;
    }
    int ret = -1;
    int uid = getuid();
    if (uid != RS_UID) {
        ret = DestroyRtgGrpAdapter(wg->rtgId);
        if (ret != 0) {
            FFRT_LOGE("[WorkGroup] destroy rtg group failed");
        } else {
            std::lock_guard<std::mutex> lck(wgLock);
            wgCount--;
        }
    }
    delete wg;
    wg = nullptr;
    return ret;
}

#endif

#if defined(QOS_FRAME_RTG)

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

    if (BeginFrameFreqAdapter(0) == 0) {
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

    int ret = EndFrameFreqAdapter(0);
    if (ret == 0) {
        wg->started = false;
    } else {
        FFRT_LOGE("[WorkGroup] stop rtg(%d) work interval failed", wg->rtgId);
    }
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
        CTC_QUERY_INTERVAL(QUERY_RENDER_SERVICE, rs);
        FFRT_LOGI("[WorkGroup] join thread %{public}ld", tid);
        return;
    }
    int addRet = AddThreadToRtgAdapter(tid, wg->rtgId, 0);
    if (addRet == 0) {
        FFRT_LOGI("[WorkGroup] join thread %{public}ld success", tid);
    } else {
        FFRT_LOGE("[WorkGroup] join fail with %{public}d threads for %{public}d", addRet, tid);
    }
}

}

#endif /* QOS_FRAME_RTG */
#endif
