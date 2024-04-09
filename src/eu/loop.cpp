#include "loop.h"
#include "queue/serial_task.h"

namespace ffrt {
Loop::Loop(SerialHandler* handler) : handler_(handler) {}

Loop::~Loop()
{
    Stop();
    handler_->ClearLoop();
}

void Loop::Run()
{
    while(!stopFlag_.load()) {
        auto task = handler_->PickUpTask();
        if (task) {
            task->Execute();

            if (!stopFlag_.load() && poller_.DeterminePollerReady()) {
                poller_.PollOnce(0);
            }
            continue;
        }

        poller_.PollOnce(-1);
    }
}

void Loop::Stop()
{
    stopFlag_.store(true);
    WakeUp();
}

void Loop::WakeUp()
{
    poller_.WakeUp();
}

int Loop::EpollCtl(int op, int fd, uint32_t events, void *data, ffrt_poller_cb cb)
{
    if (op == EPOLL_CTL_ADD) {
        return poller_.AddFdEvent(events, fd , data, cb);
    } else if (op == EPOLL_CTL_DEL) {
        return poller_.DelFdEvent(fd);
    } else if (op == EPOLL_CTL_MOD) {
        FFRT_LOGE("EPOLL_CTL_MOD not supported yet");
        return -1;
    } else {
        FFRT_LOGE("EPOLL_CTL op invalid");
        return -1;
    }
}

ffrt_timer_t Loop::TimerStart(uint64_t timeout, void *data, ffrt_timer_cb cb, bool repeat)
{
    return poller_.RegisterTimer(timeout, data, cb, repeat);
}

int Loop::TimerStop(ffrt_timer_t handle)
{
    return poller_.UnregisterTimer(handle);
}
} // ffrt