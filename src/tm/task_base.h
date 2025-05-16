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

#ifndef _TASK_BASE_H_
#define _TASK_BASE_H_
#include <atomic>
#include <vector>
#include "eu/co_routine.h"
#include "internal_inc/types.h"
#include "qos.h"
#include "sched/execute_ctx.h"
#include "internal_inc/non_copyable.h"
#include "util/time_format.h"

namespace ffrt {
static std::atomic_uint64_t s_gid(0);
static constexpr uint64_t cacheline_size = 64;

typedef struct HiTraceIdStruct {
#if __BYTE_ORDER == __BIG_ENDIAN
    uint64_t chainId : 60;
    uint64_t ver : 3;
    uint64_t valid : 1;

    uint64_t parentSpanId : 26;
    uint64_t spanId : 26;
    uint64_t flags : 12;
#elif __BYTE_ORDER == __LITTLE_ENDIAN
    uint64_t valid : 1;
    uint64_t ver : 3;
    uint64_t chainId : 60;

    uint64_t flags : 12;
    uint64_t spanId : 26;
    uint64_t parentSpanId : 26;
#else
#error "ERROR: No BIG_LITTLE_ENDIAN defines."
#endif
} HiTraceIdStruct;
constexpr uint64_t HITRACE_ID_VALID = 1;

class TaskBase : private NonCopyable {
public:
    uintptr_t reserved = 0;
    uintptr_t type = 0;
    WaitEntry fq_we; // used on fifo fast que
    TaskBase(): gid(++s_gid) {}
    virtual ~TaskBase() = default;
    const uint64_t gid; // global unique id in this process
    QoS qos_ = qos_inherit;
    std::atomic_uint32_t rc = 1; // reference count for delete
#ifdef FFRT_ASYNC_STACKTRACE
    uint64_t stackId = 0;
#endif

    struct HiTraceIdStruct traceId_ = {};

    inline int GetQos() const
    {
        return qos_();
    }

    virtual std::string GetLabel() const = 0;

    virtual void Execute() = 0;

    uint64_t createTime {0};
    uint64_t executeTime {0};
    int32_t fromTid {0};

    virtual void FreeMem() = 0;

    inline uint32_t IncDeleteRef()
    {
        auto v = rc.fetch_add(1);
        return v;
    }

    inline uint32_t DecDeleteRef()
    {
        auto v = rc.fetch_sub(1);
        if (v == 1) {
            FreeMem();
        }
        return v;
    }
};

class CoTask : public TaskBase {
public:
    CoTask() = default;
    ~CoTask() override = default;

    std::string label;
    CoWakeType coWakeType { CoWakeType::NO_TIMEOUT_WAKE };
    int cpuBoostCtxId = -1;
    WaitUntilEntry* wue = nullptr;
    // lifecycle connection between task and coroutine is shown as below:
    // |*task pending*|*task ready*|*task executing*|*task done*|*task release*|
    //                             |**********coroutine*********|
    CoRoutine* coRoutine = nullptr;
    uint64_t stack_size = STACK_SIZE;
    std::atomic<pthread_t> runningTid = 0;
    int legacyCountNum = 0; // dynamic switch controlled by set_legacy_mode api
    BlockType blockType { BlockType::BLOCK_COROUTINE }; // block type for lagacy mode changing
    std::mutex mutex_; // used in coroute
    std::condition_variable waitCond_; // cv for thread wait
    std::atomic<CoTaskStatus> curStatus = CoTaskStatus::PENDING;
    CoTaskStatus preStatus = CoTaskStatus::PENDING;
    uint64_t statusTime = TimeStampCntvct();

    // !deprecated
    void SetTraceTag(const char* name)
    {
        (void)name;
    }

    // !deprecated
    void ClearTraceTag()
    {
    }

    std::string GetLabel() const override
    {
        return label;
    }

    void SetStatus(CoTaskStatus statusIn)
    {
        statusTime = TimeStampCntvct();
        preStatus = curStatus;
        curStatus = statusIn;
    }
};
} // namespace ffrt
#endif
