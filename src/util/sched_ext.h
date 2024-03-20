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
#ifndef SCHED_EXT_H
#define SCHED_EXT_H

namespace ffrt {
struct sched_attr {
    uint32_t size;

    uint32_t sched_policy;
    uint64_t sched_flags;

    /* SCHED_NORMAL, SCHED_BATCH */
    int32_t sched_nice;

    /* SCHED_FIFO, SCHED_RR */
    uint32_t sched_priority;

    /* SCHED_DEADLINE */
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;

    /* Utiliztion hints */
    uint32_t sched_util_min;
    uint32_t sched_util_max;
};

/*
 * Scheduling policies
 */
#define SCHED_NORMAL     0
#define SCHED_FIFO       1
#define SCHED_RR         2
#define SCHED_BATCH      3
/* SCHED ISO: reserved but not implemented yet */
#define SCHED_IDLE       5
#define SCHED_DEADLINE   6

/* Can be ORed in to make sure the process is reverted back to SCHED_NORMAL on fork*/
#define SCHED_RESET_ON_FORK    0x40000000

/*
 * For the sched_{set,get}attr() calls
 */
#define SCHED_FLAG_RESET_ON_FORK    0x01
#define SCHED_FLAG_RECLAIM          0x02
#define SCHED_FLAG_DL_OVERRUN       0x04
#define SCHED_FLAG_KEEP_POLICY      0x08
#define SCHED_FLAG_KEEP_PARAMS      0x10
#define SCHED_FLAG_UTIL_CLAMP_MIN   0x20
#define SCHED_FLAG_UTIL_CLAMP_MAX   0x40

#define SCHED_FLAG_KEEP_ALL (SCHED_FLAG_KEEP_POLICY | \
                SCHED_FLAG_KEEP_PARAMS)

#define SCHED_FLAG_UTIL_CLAMP  (SCHED_FLAG_UTIL_CLAMP_MIN | \
                SCHED_FLAG_UTIL_CLAMP_MAX)

#define SCHED_FLAG_ALL  (SCHED_FLAG_RESET_ON_FORK | \
                SCHED_FLAG_RECLAIM | \
                SCHED_FLAG_DL_OVERRUN | \
                SCHED_FLAG_KEEP_ALL | \
                SCHED_FLAG_UTIL_CLAMP)
} // namepace ffrt
#endif /* SCHED_EXT_H */