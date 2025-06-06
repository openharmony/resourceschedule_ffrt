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

#include "eu/execute_unit.h"

#include "internal_inc/config.h"
#include "util/singleton_register.h"
#include "eu/co_routine_factory.h"
#include "util/ffrt_facade.h"
#include "dfx/sysevent/sysevent.h"

namespace {
const size_t MAX_ESCAPE_WORKER_NUM = 1024;
}

namespace ffrt {
ExecuteUnit::ExecuteUnit()
{
    ffrt::CoRoutineInstance(CoStackAttr::Instance()->size);

    workerGroup[qos_deadline_request].tg = std::make_unique<ThreadGroup>();

    for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
        workerGroup[qos].hardLimit = DEFAULT_HARDLIMIT;
        workerGroup[qos].maxConcurrency = GlobalConfig::Instance().getCpuWorkerNum(qos);
    }
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    memset_s(&domainInfoMonitor, sizeof(domainInfoMonitor), 0, sizeof(domainInfoMonitor));
    wakeupCond.check_ahead = false;
    wakeupCond.global.low = 0;
    wakeupCond.global.high = 0;
    for (int i = 0; i < BLOCKAWARE_DOMAIN_ID_MAX + 1; i++) {
        wakeupCond.local[i].low = 0;
        if (i < qosMonitorMaxNum) {
            wakeupCond.local[i].high = UINT_MAX;
            wakeupCond.global.low += wakeupCond.local[i].low;
            wakeupCond.global.high = UINT_MAX;
        } else {
            wakeupCond.local[i].high = 0;
        }
    }
#endif
    for (int idx = 0; idx < QoS::MaxNum(); idx++) {
        we_[idx] = new WaitUntilEntry;
        we_[idx]->cb = nullptr;
    }
}

ExecuteUnit::~ExecuteUnit()
{
    // worker escape event
    FFRT_LOGI("Destructor.");
    for (int idx = 0; idx < QoS::MaxNum(); idx++) {
        if (we_[idx] != nullptr) {
            delete we_[idx];
            we_[idx] = nullptr;
        }
    }
}

ExecuteUnit &ExecuteUnit::Instance()
{
    return SingletonRegister<ExecuteUnit>::Instance();
}

void ExecuteUnit::RegistInsCb(SingleInsCB<ExecuteUnit>::Instance &&cb)
{
    SingletonRegister<ExecuteUnit>::RegistInsCb(std::move(cb));
}

ThreadGroup *ExecuteUnit::BindTG(QoS& qos)
{
    auto &tgwrap = workerGroup[qos];
    if (!tgwrap.tg) {
        return nullptr;
    }

    std::unique_lock<std::shared_mutex> lck(tgwrap.tgMutex);

    if (tgwrap.tgRefCount++ > 0) {
        return tgwrap.tg.get();
    }

    if (!(tgwrap.tg->Init())) {
        FFRT_SYSEVENT_LOGE("Init Thread Group Failed");
        return tgwrap.tg.get();
    }

    for (auto &thread : tgwrap.threads) {
        pid_t tid = thread.first->Id();
        if (!(tgwrap.tg->Join(tid))) {
            FFRT_SYSEVENT_LOGE("Failed to Join Thread %d", tid);
        }
    }
    return tgwrap.tg.get();
}

void ExecuteUnit::BindWG(QoS& qos)
{
    auto &tgwrap = workerGroup[qos];
    std::shared_lock<std::shared_mutex> lck(tgwrap.tgMutex);
    for (auto &thread : tgwrap.threads) {
        pid_t tid = thread.first->Id();
        if (!JoinWG(tid, qos)) {
            FFRT_SYSEVENT_LOGE("Failed to Join Thread %d", tid);
        }
    }
}

void ExecuteUnit::UnbindTG(QoS& qos)
{
    auto &tgwrap = workerGroup[qos];
    if (!tgwrap.tg) {
        return;
    }

    std::unique_lock<std::shared_mutex> lck(tgwrap.tgMutex);

    if (tgwrap.tgRefCount == 0) {
        return;
    }

    if (--tgwrap.tgRefCount == 0) {
        if (qos != qos_user_interactive) {
            for (auto &thread : tgwrap.threads) {
                pid_t tid = thread.first->Id();
                if (!(tgwrap.tg->Leave(tid))) {
                    FFRT_SYSEVENT_LOGE("Failed to Leave Thread %d", tid);
                }
            }
        }

        if (!(tgwrap.tg->Release())) {
            FFRT_SYSEVENT_LOGE("Release Thread Group Failed");
        }
    }
}

int ExecuteUnit::SetWorkerStackSize(const QoS &qos, size_t stack_size)
{
    CPUWorkerGroup &group = workerGroup[qos];
    std::unique_lock<std::shared_mutex> lck(group.tgMutex);
    if (!group.threads.empty()) {
        FFRT_SYSEVENT_LOGE("stack size can be set only when there is no worker.");
        return -1;
    }
    int pageSize = getpagesize();
    if (pageSize < 0) {
        FFRT_SYSEVENT_LOGE("Invalid pagesize : %d", pageSize);
        return -1;
    }
    group.workerStackSize = (stack_size - 1 + static_cast<size_t>(pageSize)) & -(static_cast<size_t>(pageSize));
    return 0;
}

int ExecuteUnit::SetEscapeEnable(uint64_t oneStageIntervalMs, uint64_t twoStageIntervalMs,
    uint64_t threeStageIntervalMs, uint64_t oneStageWorkerNum, uint64_t twoStageWorkerNum)
{
    if (escapeConfig.enableEscape_) {
        FFRT_LOGW("Worker escape is enabled, the interface cannot be invoked repeatedly.");
        return 1;
    }

    if (oneStageIntervalMs < escapeConfig.oneStageIntervalMs_ ||
        twoStageIntervalMs < escapeConfig.twoStageIntervalMs_ ||
        threeStageIntervalMs < escapeConfig.threeStageIntervalMs_ || oneStageWorkerNum > twoStageWorkerNum) {
        FFRT_LOGE("Setting failed, each stage interval value [%lu, %lu, %lu] "
                  "cannot be smaller than default value [%lu, %lu, %lu], "
                  "and one-stage worker number [%lu] cannot be larger than two-stage worker number [%lu].",
            oneStageIntervalMs,
            twoStageIntervalMs,
            threeStageIntervalMs,
            escapeConfig.oneStageIntervalMs_,
            escapeConfig.twoStageIntervalMs_,
            escapeConfig.threeStageIntervalMs_,
            oneStageWorkerNum,
            twoStageWorkerNum);
        return 1;
    }

    escapeConfig.enableEscape_ = true;
    escapeConfig.oneStageIntervalMs_ = oneStageIntervalMs;
    escapeConfig.twoStageIntervalMs_ = twoStageIntervalMs;
    escapeConfig.threeStageIntervalMs_ = threeStageIntervalMs;
    escapeConfig.oneStageWorkerNum_ = oneStageWorkerNum;
    escapeConfig.twoStageWorkerNum_ = twoStageWorkerNum;
    FFRT_LOGI("Enable worker escape success, one-stage interval ms %lu, two-stage interval ms %lu, "
              "three-stage interval ms %lu, one-stage worker number %lu, two-stage worker number %lu.",
        escapeConfig.oneStageIntervalMs_,
        escapeConfig.twoStageIntervalMs_,
        escapeConfig.threeStageIntervalMs_,
        escapeConfig.oneStageWorkerNum_,
        escapeConfig.twoStageWorkerNum_);
    return 0;
}

void ExecuteUnit::SubmitEscape(int qos, uint64_t totalWorkerNum)
{
    // escape event has been triggered and will not be submitted repeatedly
    if (submittedDelayedTask_[qos]) {
        return;
    }

    we_[qos]->tp = std::chrono::steady_clock::now() + std::chrono::milliseconds(CalEscapeInterval(totalWorkerNum));
    if (we_[qos]->cb == nullptr) {
        we_[qos]->cb = [this, qos](WaitEntry *we) {
            (void)we;
            ExecuteEscape(qos);
            submittedDelayedTask_[qos] = false;
        };
    }

    if (!DelayedWakeup(we_[qos]->tp, we_[qos], we_[qos]->cb)) {
        FFRT_LOGW("Failed to set qos %d escape task.", qos);
        return;
    }

    submittedDelayedTask_[qos] = true;
}

std::array<sched_mode_type, QoS::MaxNum()> ExecuteUnit::schedMode{};

bool ExecuteUnit::IncWorker(const QoS &qos)
{
    int workerQos = qos();
    if (workerQos < 0 || workerQos >= QoS::MaxNum()) {
        FFRT_SYSEVENT_LOGE("IncWorker qos:%d is invaild", workerQos);
        return false;
    }
    if (tearDown) {
        FFRT_SYSEVENT_LOGE("CPU Worker Manager exit");
        return false;
    }

    workerNum.fetch_add(1);
    auto worker = CreateCPUWorker(qos);
    auto uniqueWorker = std::unique_ptr<CPUWorker>(worker);
    if (uniqueWorker == nullptr) {
        workerNum.fetch_sub(1);
        FFRT_SYSEVENT_LOGE("IncWorker failed: worker is nullptr\n");
        return false;
    }
    {
        std::lock_guard<std::shared_mutex> lock(workerGroup[workerQos].tgMutex);
        if (uniqueWorker->Exited()) {
            FFRT_SYSEVENT_LOGW("IncWorker failed: worker has exited\n");
            goto create_success;
        }

        auto result = workerGroup[workerQos].threads.emplace(worker, std::move(uniqueWorker));
        if (!result.second) {
            FFRT_SYSEVENT_LOGW("qos:%d worker insert fail:%d", workerQos, result.second);
        }
    }
create_success:
#ifdef FFRT_WORKER_MONITOR
    FFRTFacade::GetWMInstance().SubmitTask();
#endif
    FFRTTraceRecord::UseFfrt();
    return true;
}

void ExecuteUnit::RestoreThreadConfig()
{
    for (auto qos = ffrt::QoS::Min(); qos < ffrt::QoS::Max(); ++qos) {
        ffrt::CPUWorkerGroup &group = workerGroup[qos];
        std::unique_lock<std::shared_mutex> lck(group.tgMutex);
        for (auto &thread : group.threads) {
            thread.first->SetThreadAttr(qos);
        }
    }
}

void ExecuteUnit::NotifyWorkers(const QoS &qos, int number)
{
    CPUWorkerGroup &group = workerGroup[qos];
    group.lock.lock();

    int increasableNumber = static_cast<int>(group.maxConcurrency) - (group.executingNum + group.sleepingNum);
    int wakeupNumber = std::min(number, group.sleepingNum);
    for (int idx = 0; idx < wakeupNumber; idx++) {
        WakeupWorkers(qos);
    }

    int incNumber = std::min(number - wakeupNumber, increasableNumber);
    for (int idx = 0; idx < incNumber; idx++) {
        group.executingNum++;
        IncWorker(qos);
    }

    group.lock.unlock();
    FFRT_LOGD("qos[%d] inc [%d] workers, wakeup [%d] workers", static_cast<int>(qos), incNumber, wakeupNumber);
}

void ExecuteUnit::WorkerRetired(CPUWorker *thread)
{
    thread->SetWorkerState(WorkerStatus::DESTROYED);
    pid_t pid = thread->Id();
    int qos = static_cast<int>(thread->GetQos());

    {
        std::unique_lock<std::shared_mutex> lck(workerGroup[qos].tgMutex);
        thread->SetExited(true);
        thread->Detach();
        auto worker = std::move(workerGroup[qos].threads[thread]);
        int ret = workerGroup[qos].threads.erase(thread);
        if (ret != 1) {
            FFRT_SYSEVENT_LOGE("erase qos[%d] thread failed, %d elements removed", qos, ret);
        }
        WorkerLeaveTg(qos, pid);
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        if (IsBlockAwareInit()) {
            ret = BlockawareUnregister();
            if (ret != 0) {
                FFRT_SYSEVENT_LOGE("blockaware unregister fail, ret[%d]", ret);
            }
        }
#endif
        worker = nullptr;
        workerNum.fetch_sub(1);
    }
}

PollerRet ExecuteUnit::TryPoll(const CPUWorker *thread, int timeout)
{
    if (tearDown || FFRTFacade::GetPPInstance().GetPoller(thread->GetQos()).DetermineEmptyMap()) {
        return PollerRet::RET_NULL;
    }
    CPUWorkerGroup &group = workerGroup[thread->GetQos()];
    if (group.pollersMtx.try_lock()) {
        /* The required orders on these operations are not clear from the code.
         * To ensure correctness, we try to enforce all ordering (by preserving program oder).
         * We forbid moving the updates to polling_ into the critical sections, e.g., of IntoPollWait.
         * This is achieved by using matching seq_cst order on the first store and the atomic load
         * (see SExecuteUnit::WorkerIdleAction),
         * which ensures the atomic store can't be observed to occur
         * in the critical section of IntoPollWait, and a release barrier
         * on the second store to ensure it can not be observed to have been moved up.
         */
        group.polling_ = true;
        if (timeout == -1) {
            IntoPollWait(thread->GetQos());
        }
        PollerRet ret = FFRTFacade::GetPPInstance().GetPoller(thread->GetQos()).PollOnce(timeout);
        if (timeout == -1) {
            workerGroup[thread->GetQos()].OutOfPollWait();
        }
        /* release barrier is used here to ensure this write does not move up */
        group.polling_.store(false, std::memory_order_release);
        group.pollersMtx.unlock();
        return ret;
    }
    return PollerRet::RET_NULL;
}

void ExecuteUnit::WorkerJoinTg(const QoS &qos, pid_t pid)
{
    std::shared_lock<std::shared_mutex> lock(workerGroup[qos()].tgMutex);
    if (qos == qos_user_interactive || qos > qos_max) {
        (void)JoinWG(pid, qos);
        return;
    }
    auto &tgwrap = workerGroup[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Join(pid);
}

void ExecuteUnit::WorkerLeaveTg(const QoS &qos, pid_t pid)
{
    if (qos == qos_user_interactive || qos > qos_max) {
        (void)LeaveWG(pid, qos);
        return;
    }
    auto &tgwrap = workerGroup[qos()];
    if (!tgwrap.tg) {
        return;
    }

    if ((tgwrap.tgRefCount) == 0) {
        return;
    }

    tgwrap.tg->Leave(pid);
}

CPUWorker *ExecuteUnit::CreateCPUWorker(const QoS &qos)
{
    // default strategy of worker ops
    CpuWorkerOps ops{
        [this](CPUWorker *thread) { return this->WorkerIdleAction(thread); },
        [this](CPUWorker *thread) { this->WorkerRetired(thread); },
        [this](CPUWorker *thread) { this->WorkerPrepare(thread); },
        [this](const CPUWorker *thread, int timeout) { return this->TryPoll(thread, timeout); },
#ifdef FFRT_WORKERS_DYNAMIC_SCALING
        [this]() { return this->IsBlockAwareInit(); },
#endif
    };

    return new (std::nothrow) CPUWorker(qos, std::move(ops), workerGroup[qos].workerStackSize);
}

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
bool ExecuteUnit::IsBlockAwareInit()
{
    return blockAwareInit;
}

BlockawareWakeupCond *ExecuteUnit::WakeupCond(void)
{
    return &wakeupCond;
}

void ExecuteUnit::MonitorMain()
{
    (void)WorkerInit();
    int ret = BlockawareLoadSnapshot(keyPtr, &domainInfoMonitor);
    if (ret != 0) {
        FFRT_SYSEVENT_LOGE("blockaware load snapshot fail, ret[%d]", ret);
        return;
    }
    for (int i = 0; i < qosMonitorMaxNum; i++) {
        auto &info = domainInfoMonitor.localinfo[i];
        if (info.nrRunning <= wakeupCond.local[i].low &&
            (info.nrRunning + info.nrBlocked + info.nrSleeping) < MAX_ESCAPE_WORKER_NUM) {
            NotifyTask<TaskNotifyType::TASK_ESCAPED>(i);
        }
    }
    stopMonitor = true;
}
#endif

size_t ExecuteUnit::GetRunningNum(const QoS &qos)
{
    CPUWorkerGroup &group = workerGroup[qos()];
    size_t runningNum = group.executingNum;

#ifdef FFRT_WORKERS_DYNAMIC_SCALING
    /* There is no need to update running num when executingNum < maxConcurrency */
    if (static_cast<size_t>(group.executingNum) >= group.maxConcurrency && blockAwareInit) {
        auto nrBlocked = BlockawareLoadSnapshotNrBlockedFast(keyPtr, qos());
        if (static_cast<unsigned int>(group.executingNum) >= nrBlocked) {
            /* nrRunning may not be updated in a timely manner */
            runningNum = group.executingNum - nrBlocked;
        } else {
            FFRT_SYSEVENT_LOGE(
                "qos [%d] nrBlocked [%u] is larger than executingNum [%d].", qos(), nrBlocked, group.executingNum);
        }
    }
#endif

    return runningNum;
}

void ExecuteUnit::ReportEscapeEvent(int qos, size_t totalNum)
{
#ifdef FFRT_SEND_EVENT
    WorkerEscapeReport(GetCurrentProcessName(), qos, totalNum);
#endif
}
} // namespace ffrt
