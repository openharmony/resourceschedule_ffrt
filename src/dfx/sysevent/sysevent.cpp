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
#ifdef FFRT_SEND_EVENT
#include "sysevent.h"
#include "hisysevent.h"

namespace ffrt {
void TaskTimeoutReport(std::stringstream& ss, std::string& processNameStr, std::string& senarioName)
{
    std::string msg = ss.str();
    std::string eventName = "TASK_TIMEOUT";
    time_t cur_time = time(nullptr);
    std::string sendMsg = std::string((ctime(&cur_time) == nullptr) ? "" : ctime(&cur_time)) + "\n" + msg + "\n";
    HiSysEventWrite(OHOS::HiviewDFX::HiSysEvent::Domain::FFRT, eventName,
        OHOS::HiviewDFX::HiSysEvent::EventType::FAULT, "SENARIO", senarioName,
        "PROCESS_NAME", processNameStr, "MSG", sendMsg);
}
}
#endif