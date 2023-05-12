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
#ifndef FFRT_SERIAL_LOOPER_H
#define FFRT_SERIAL_LOOPER_H

#include <atomic>
#include <memory>
#include <string>
#include "c/type_def.h"
#include "internal_inc/non_copyable.h"
#include "queue/serial_queue.h"

namespace ffrt {
class SerialLooper : public NonCopyable {
public:
    explicit SerialLooper(const char* name, enum qos qos);
    ~SerialLooper();
    void Quit();
    std::shared_ptr<SerialQueue> m_queue;

private:
    void Run();
    std::string name_ = "serial_queue_";
    std::atomic_bool isExit_ = {false};
    ffrt_task_handle_t handle = nullptr;
};
} // namespace ffrt

#endif // FFRT_SERIAL_LOOPER_H
