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

#include "eu/worker_manager.h"
#include "sched/workgroup_internal.h"

namespace ffrt {
void WorkerManager::JoinRtg(QoS& qos)
{
    auto& tgwrap = groupCtl[qos];
    std::shared_lock<std::shared_mutex> lck(tgwrap.tgMutex);
    for (auto& thread : tgwrap.threads) {
        pid_t tid = thread.first->Id();
        if (!JoinWG(tid)) {
            FFRT_LOGE("Failed to Join Thread %d", tid);
        }
    }
}

ThreadGroup* WorkerManager::JoinTG(QoS& qos)
{
    auto& tgwrap = groupCtl[qos];
    if (!tgwrap.tg) {
        return nullptr;
    }

    std::unique_lock<std::shared_mutex> lck(tgwrap.tgMutex);

    if (tgwrap.tgRefCount++ > 0) {
        return tgwrap.tg.get();
    }

    if (!(tgwrap.tg->Init())) {
        FFRT_LOGE("Init Thread Group Failed");
        return tgwrap.tg.get();
    }

    for (auto& thread : tgwrap.threads) {
        pid_t tid = thread.first->Id();
        if (!(tgwrap.tg->Join(tid))) {
            FFRT_LOGE("Failed to Join Thread %d", tid);
        }
    }
    return tgwrap.tg.get();
}

void WorkerManager::LeaveTG(QoS& qos)
{
    auto& tgwrap = groupCtl[qos];
    if (!tgwrap.tg) {
        return;
    }

    std::unique_lock<std::shared_mutex> lck(tgwrap.tgMutex);

    if (tgwrap.tgRefCount == 0) {
        return;
    }

    if (--tgwrap.tgRefCount == 0) {
        if (qos != qos_user_interactive) {
            for (auto& thread : groupCtl[qos].threads) {
                pid_t tid = thread.first->Id();
                if (!(tgwrap.tg->Leave(tid))) {
                    FFRT_LOGE("Failed to Leave Thread %d", tid);
                }
            }
        }

        if (!(tgwrap.tg->Release())) {
            FFRT_LOGE("Release Thread Group Failed");
        }
    }
}
}; // namespace ffrt
