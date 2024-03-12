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

#ifndef _SCPU_TASK_H_
#define _SCPU_TASK_H_

#include "tm/cpu_task.h"

namespace ffrt {
class SCPUEUTask : public CPUEUTask {
public:
    SCPUEUTask(const task_attr_private *attr, CPUEUTask *parent, const uint64_t &id, const QoS &qos = QoS());
    std::unordered_set<VersionCtx*> ins;
    std::unordered_set<VersionCtx*> outs;
    std::vector<CPUEUTask*> in_handles;

    std::mutex denpenceStatusLock;
    Denpence denpenceStatus {Denpence::DEPENCE_INIT};

    std::atomic_uint64_t depRefCnt {0};

    std::atomic_uint64_t childWaitRefCnt {0};
    std::condition_variable childWaitCond_;

    uint64_t dataWaitRefCnt {0}; // waited data count called by ffrt_wait()
    std::condition_variable dataWaitCond_; // wait data cond

    inline void IncDepRef()
    {
        ++depRefCnt;
    }
    void DecDepRef();

    inline void IncChildRef()
    {
        ++(static_cast<SCPUEUTask*>(parent)->childWaitRefCnt);
    }
    void DecChildRef();

    inline void IncWaitDataRef()
    {
        ++dataWaitRefCnt;
    }
    void DecWaitDataRef();
    void MultiDepenceAdd(Denpence depType);
    void RecycleTask() override;
};

class RootTask : public SCPUEUTask {
public:
    RootTask(const task_attr_private* attr, SCPUEUTask* parent, const uint64_t& id,
        const QoS& qos = QoS()) : SCPUEUTask(attr, parent, id, qos)
    {
    }
public:
    bool thread_exit = false;
};

class RootTaskCtxWrapper {
public:
    RootTaskCtxWrapper()
    {
        task_attr_private task_attr;
        root = new RootTask{&task_attr, nullptr, 0};
    }
    ~RootTaskCtxWrapper()
    {
        std::unique_lock<decltype(root->lock) > lck(root->lock);
        if (root->childWaitRefCnt == 0) {
            lck.unlock();
            delete root;
        } else {
            root->thread_exit = true;
        }
    }
    CPUEUTask* Root()
    {
        return root;
    }
private:
    RootTask *root = nullptr;
};
} /* namespace ffrt */
#endif
