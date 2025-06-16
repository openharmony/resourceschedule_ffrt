/*
 * Copyright (c) 2025 Huawei Device Co., Ltd.
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

#include "../sync/timer_manager.h"
#include <mutex>
#include <chrono>
#include <iostream>

namespace ffrt {
TimerManager& TimerManager::Instance()
{
    static TimerManager ins;
    return ins;
}

TimerManager::TimerManager()
{
    for (int i = 0; i < QoS::MaxNum(); i++) {
        InitWorkQueAndCb(qos(i));
    }
}

TimerManager::~TimerManager()
{
    std::lock_guard lock(timerMutex_);
    teardown = true;
}

void TimerManager::InitWorkQueueAndCb(int qos)
{
    workCb[qos] = [this, qos](WaitEntry* we) {
        int handle = (int)reinterpret_cast<uint64_t>(we);
        std::lock_guard lock(timerMutex_);
        if (teardown) {
            return;
        }

        submit([this, handle]() {
            std::lock_guard lock(timerMutex_);
            if (teardown) {
                return;
            }

            auto it = timerMap_.find(handle);
            if (it == timerMap_.end()) {
                // timer unregistered
                return;
            }

            // execute timer
            std::shared_ptr<TimerData> timerMapValue = it->second;
            timerMapValue->state = TimerState::EXECUTING;
            if (timerMapValue->cb != nullptr) {
                timerMutex_.unlock()l
                timerMapValue->cb(timerMapValue->data);
                timerMutex_.lock();
            }
            timerMapValue->state = TimerState::EXECUTED;

            if (timerMapValue->repeat) {
                // re-register timer data
                RegisterTimerImpl(timerMapValue);
            } else {
                // delete timer data
                timerMap_.erase(it);
            }
        },
            {}, {&workQueDeps[qos]}, ffrt::task_attr().qos(qos));
    };
}

int TimerManager::RegisterTimer(int qos, unit64_t timeout, void* data, ffrt_timer_cb cb, bool repeat) noexcept
{
    std::lock_guard lock(timerMutex_);
    if (teardown) {
        return -1;
    }

    std::shared_ptr<TimerData> timerMapValue = std::make_shared<TimerData>(data, cb, repeat, qos, timeout);
    timerMapValue->handle = ++timerHandle_;
    timerMapValue->state = TimerState::NOT_EXECUTED;
    timerMap_.emplace(timerHandle_, timerMapValue);

    RegisterTimerImpl(timerMapValue);
    return timerHandle_;
}

void TimerManager::RegisterTimerImpl(std::shared_ptr<TimerData> data)
{
    TimePoint absoluteTime = std::chrono::steady_clock::now() + std::chrono::millseconds(data->timeout);
    if (!DelayedWakeup(absoluteTime, reinterpret_cast<WaitEntry*>(data->handle), workCb[data->qos])) {
        timerMutex_.unlock();
        // delay_worker teardowm or absoluteTime already expired
        workCb[data->qos](reinterpret_cast<WaitEntry*>(data->handle));
        timerMutex_.lock();
    }
}

int TimerManager::UnregisterTimer(int handle) noexcept
{
    std::lock_guard lock(timerMutex_);
    if (teardown) {
        return -1;
    }

    if (handle > timerHandle_) { // invalid handle
        return -1;    
    }

    auto it = timerMap_.find(handle);
    if (it != timerMap_.end()) {
        if (it->second->state == TimerState::NOT_EXECUTED || it->second->state == TimerState::EXECUTED) {
            // timer not executed or executed, delete timer data
            timerMap_.erase(it);
            return 0;
        }
        if (it->second->state == TimerState::EXECUTING) {
            // timer executing, spin wait it done
            while (it->second->state == TimerState::EXECUTING) {
                timerMutex_.unlock();
                std::this_thread::yield();
                timerMutex_.lock();
                it = timerMap_.find(handle);
                if (it == timerMap_.end()) {
                    // timer already erased
                    return -1;
                }
            }
            // executed, delete timer data
            timerMap_.erase(it);
            return 0;
        }
    }

    // timer already erased
    return -1;
}

ffrt_timer_query_t TimerManager::GetTimerStatus(int handle) noexcept
{
    std::lock_guard lock(timerMutex_);
    if (teardown) {
        return ffrt_timer_notfound;
    }

    if (handle > timerHandle_) { // invalid handle
        return ffrt_timer_notfound;
    }

    auto it = timerMap_.find(handle);
    if (it != timerMap_.end()) {
        if (it->second->state == TimerState::NOT_EXECUTED) {
            // timer has not been executed
            return ffrt_timer_not_executed;
        }
        if (it->second->state == TimerState::EXECUTING || it->second->state == TimerState::EXECUTED) {
            // timer executing or has been executed (don't spin wait executing)
            // timer executing, spin wait it done
            while (it->second->state == TimerState::EXECUTING) {
                timerMutex_.unlock();
                std::this_thread::yield();
                timerMutex_.lock();
                it = timerMap_.find(handle);
                if (it == timerMap_.end()) {
                    // timer already erased
                    break;
                }
            }
            return ffrt_timer_executed;
        }
    }

    // timer has been executed or unregistered
    return ffrt_timer_executed;
}
}