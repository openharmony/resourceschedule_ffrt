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