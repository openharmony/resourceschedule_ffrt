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

#ifdef QOS_SINGLE_RTG

#include <stdio.h>
#include <stdlib.h>

#include "workgroup_internal.h"
#include "rtg_perf_ctrl.h"

void workgroup_start_interval(workgroup_t wg)
{
    if (wg->started) {
        return;
    }
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx starts interval %s\n", gettid(), wg->wg_label);
#endif
    wg->started = true;
    set_rtg_status(FRAME_START);
}

void workgroup_stop_interval(workgroup_t wg)
{
    if (!wg->started) {
        return;
    }
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx stops interval %s\n", gettid(), wg->wg_label);
#endif
    set_rtg_status(FRAME_END);
    wg->started = false;
}

workgroup_t workgroup_create(const char* label, long interval __attribute__((unused)))
{
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx creates wg %s\n", gettid(), label);
#endif
    workgroup_t wg = (workgroup_t)malloc(sizeof(struct workgroup_s));
    if (wg == NULL) {
        return wg;
    }

    wg->wg_label = label;
    wg->started = false;
    wg->autoStart = false;
    wg->interval = interval;
    wg->rtg_id = DEFAULT_RT_FRAME_ID;
    set_rtg_load_mode(wg->rtg_id, true, false);

    return wg;
}

void workgroup_dispose(workgroup_t wg)
{
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx frees wg %s\n", gettid(), wg->wg_label);
#endif
    free(wg);
}

void workgroup_update_interval(workgroup_t wg, long interval)
{
#if QOS_WORKGROUP_DEBUG
    printf("set wg %s interval %ld\n", wg->wg_label, interval);
#endif
    set_rtg_qos(1000000 / interval);
}

void workgroup_update(workgroup_t wg __attribute__((unused)), bool join __attribute__((unused)))
{
}

void workgroup_update_tid(
    workgroup_t wg __attribute__((unused)), bool join __attribute__((unused)), long tid __attribute__((unused)))
{
}

void workgroup_join(workgroup_t wg __attribute__((unused)))
{
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx join wg %s\n", gettid(), wg->wg_label);
#endif
    set_task_rtg(gettid(), wg->rtg_id);
}

void workgroup_join_tid(workgroup_t wg __attribute__((unused)), long tid __attribute__((unused)))
{
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx join wg %s\n", tid, wg->wg_label);
#endif
    set_task_rtg(tid, wg->rtg_id);
}

void workgroup_leave(workgroup_t wg __attribute__((unused)))
{
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx leave wg %s\n", gettid(), wg->wg_label);
#endif
}

void workgroup_clear(workgroup_t wg __attribute__((unused)))
{
#if QOS_WORKGROUP_DEBUG
    printf("thread %lx clear wg %s\n", gettid(), wg->wg_label);
#endif
}

void workgroup_checkpoint(workgroup_t wg, int cpid)
{
#if QOS_WORKGROUP_DEBUG
    printf("workgroup %d(%s) checkpoint %d trigger\n", wg->rtg_id, wg->wg_label, cpid);
#endif
    // TODO
}

#endif /* QOS_SINGLE_RTG */
