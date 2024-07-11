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

#ifndef WORKGROUP_INCLUDE
#define WORKGROUP_INCLUDE

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <cstdbool>
#include <chrono>
#include <fcntl.h>
#include <string>

constexpr int RS_UID = 1003;
constexpr int MAX_WG_THREADS = 32;
#define MAX_FRAME_BUFFER 6
#define gettid() syscall(SYS_gettid)
namespace ffrt {
enum WgType {
    TYPE_DEFAULT = 0,
    TYPE_RS = 1,
    TYPE_MAX
};

struct Workgroup {
    bool started;
    int rtgId;
    int tids[MAX_WG_THREADS];
    uint64_t interval;
    WgType type;
};

#if defined(QOS_WORKER_FRAME_RTG)
struct Workgroup* WorkgroupCreate(uint64_t interval);
int WorkgroupClear(struct Workgroup* wg);
bool JoinWG(int tid);
bool LeaveWG(int tid);
#else

inline struct Workgroup* WorkgroupCreate(uint64_t interval __attribute__((unused)))
{
    struct Workgroup* wg = new (std::nothrow) struct Workgroup();
    if (wg == nullptr) {
        return nullptr;
    }
    return wg;
} 

inline int WorkgroupClear(struct Workgroup* wg)
{
    delete wg;
    wg = nullptr;
    return 0;
}

inline bool JoinWG(int tid)
{
    (void)tid;
    return true;
}

inline bool LeaveWG(int tid)
{
    (void)tid;
    return true;
}

#endif

#if defined(QOS_FRAME_RTG)

void WorkgroupStartInterval(struct Workgroup* wg);
void WorkgroupStopInterval(struct Workgroup* wg);
void WorkgroupJoin(struct Workgroup* wg, int tid);

#else /* !QOS_FRAME_RTG */

inline void WorkgroupStartInterval(struct Workgroup* wg)
{
    if (wg->started) {
        return;
    }
    wg->started = true;
}

inline void WorkgroupStopInterval(struct Workgroup* wg)
{
    if (!wg->started) {
        return;
    }
    wg->started = false;
}

inline void WorkgroupJoin(struct Workgroup* wg, int tid)
{
    (void)wg;
    (void)tid;
}

#endif /* QOS_FRAME_RTG */
}
#endif
