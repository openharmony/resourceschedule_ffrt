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
#ifndef UTIL_FFRTFACADE_HPP
#define UTIL_FFRTFACADE_HPP
#include "tm/cpu_task.h"
#include "sched/scheduler.h"
#include "eu/execute_unit.h"
#include "dm/dependence_manager.h"
#include "sync/poller.h"
#include "sync/delayed_worker.h"
namespace ffrt {
bool GetExitFlag();
std::shared_mutex& GetExitMtx();

class FFRTFacade {
public:
    static inline ExecuteUnit& GetEUInstance()
    {
        static ExecuteUnit& inst = Instance().GetEUInstanceImpl();
        return inst;
    }

    static inline DependenceManager& GetDMInstance()
    {
        static DependenceManager& inst = Instance().GetDMInstanceImpl();
        return inst;
    }

    static inline PollerProxy& GetPPInstance()
    {
        PollerProxy& inst = Instance().GetPPInstanceImpl();
        return inst;
    }

    static inline DelayedWorker& GetDWInstance()
    {
        DelayedWorker& inst = Instance().GetDWInstanceImpl();
        return inst;
    }

    static inline FFRTScheduler* GetSchedInstance()
    {
        FFRTScheduler* inst = Instance().GetSchedInstanceImpl();
        return inst;
    }

private:
    FFRTFacade();

    static FFRTFacade& Instance()
    {
        static FFRTFacade facade;
        return facade;
    }

    inline ExecuteUnit& GetEUInstanceImpl()
    {
        return ExecuteUnit::Instance();
    }

    inline DependenceManager& GetDMInstanceImpl()
    {
        return DependenceManager::Instance();
    }

    inline PollerProxy& GetPPInstanceImpl()
    {
        return PollerProxy::Instance();
    }

    inline DelayedWorker& GetDWInstanceImpl()
    {
        return DelayedWorker::GetInstance();
    }

    inline FFRTScheduler* GetSchedInstanceImpl()
    {
        return FFRTScheduler::Instance();
    }
};

} // namespace FFRT
#endif