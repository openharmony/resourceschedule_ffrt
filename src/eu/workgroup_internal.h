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

#ifndef __WORKGROUP_INCLUDE__
#define __WORKGROUP_INCLUDE__

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cstdlib>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WG_THREADS 32
#define LABEL_MAX 16

#if (!defined OHOS_STANDARD_SYSTEM && !defined OHOS_PLATFORM)
#define gettid() syscall(SYS_gettid)
#endif

typedef struct workgroup_s {
    int volatile ref_cnt;
    const char* wg_label;
    bool autoStart;
    bool started;
    int fd;
    int rtg_id;
    int tids[MAX_WG_THREADS];
    long interval;
    int tidNumber;
    volatile int tidStatus;
} *workgroup_t;

#if defined(QOS_SINGLE_RTG) || defined(QOS_MULTI_RTG)
// 创建 WORKGROUP
workgroup_t workgroup_create(const char* label, long interval);
// 销毁 WORKGROUP
void workgroup_dispose(workgroup_t wg);
// 更新 WORKGROUP interval
void workgroup_update_interval(workgroup_t wg, long interval);
// interval 窗口开始
void workgroup_start_interval(workgroup_t wg);
// interval 窗口结束
void workgroup_stop_interval(workgroup_t wg);
// 将当前进程 gettid() 加入 WORKGROUP
void workgroup_join(workgroup_t wg);
// 将指定进程 tid 加入 WORKGROUP
void workgroup_join_tid(workgroup_t wg, long tid);
// 更新 WORKGROUP
void workgroup_update(workgroup_t wg, bool join);
// 将指定进程 tid 加或者移除 WORKGROUP
void workgroup_update_tid(workgroup_t wg, bool join, long tid);
// 将当前进程 gettid() 移除 WORKGROUP
void workgroup_leave(workgroup_t wg);
// 清除当前 WORKGROUP 中的所有进程
void workgroup_clear(workgroup_t wg);
// checkpoint 设计
void workgroup_checkpoint(workgroup_t wg, int cpid);

#else /* !QOS_RTG */

static inline void workgroup_start_interval(workgroup_t wg)
{
    if (wg->started) {
        return;
    }
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("thread %lx starts interval %s\n", gettid(), wg->wg_label);
#endif
    wg->started = true;
}

static inline void workgroup_stop_interval(workgroup_t wg)
{
    if (!wg->started) {
        return;
    }
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("thread %lx stops interval %s\n", gettid(), wg->wg_label);
#endif
    wg->started = false;
}

static inline void workgroup_update_interval(workgroup_t wg, long interval)
{
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("wg %s update interval %lu\n", wg->wg_label, interval);
#endif
}

static inline workgroup_t workgroup_create(const char* label, long interval __attribute__((unused)))
{
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("thread %lx creates wg %s\n", gettid(), label);
#endif
    workgroup_t wg = (workgroup_t)malloc(sizeof(struct workgroup_s));
    if (wg == nullptr) {
        return wg;
    }
    wg->wg_label = label;
    return wg;
}

static inline void workgroup_dispose(workgroup_t wg)
{
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("thread %lx frees wg %s\n", gettid(), wg->wg_label);
#endif
    free(wg);
}

static inline void workgroup_join(workgroup_t wg __attribute__((unused)))
{
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("thread %lx frees wg %s\n", gettid(), wg->wg_label);
#endif
}
// 清除当前 WORKGROUP 中的所有进程
static inline void workgroup_clear(workgroup_t wg)
{
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("workgroup %d(%s) clear\n", wg->rtg_id, wg->wg_label);
#endif
}

// checkpoint 设计
static inline void workgroup_checkpoint(workgroup_t wg __attribute__((unused)), int cpid __attribute__((unused)))
{
#if defined(QOS_WORKGROUP_DEBUG) && QOS_WORKGROUP_DEBUG
    printf("workgroup %d(%s) checkpoint %d trigger\n", wg->rtg_id, wg->wg_label, cpid);
#endif
}
#endif /* QOS_RTG */

#ifdef __cplusplus
}
#endif

#endif
