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
#ifndef FFRT_SERIAL_TASK_H
#define FFRT_SERIAL_TASK_H

#include <atomic>
#include <functional>
#include "cpp/condition_variable.h"
#include "queue/itask.h"

namespace ffrt {
class IHandler;
class SerialTask : public ITask {
public:
    void Wait() override;
    void Notify() override;
    void IncDeleteRef() override;
    void DecDeleteRef() override;
    ITask* SetQueHandler(IHandler* handler) override;

private:
    ffrt::mutex mutex_;
    ffrt::condition_variable cond_;
    bool m_isFinished = false;
    std::atomic_int rc = {1};
};
} // namespace ffrt

#endif // FFRT_SERIAL_TASK_H
