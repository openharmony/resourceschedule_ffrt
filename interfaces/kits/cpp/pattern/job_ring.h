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

/**
 * @addtogroup FFRT
 * @{
 *
 * @brief Provides FFRT C++ APIs.
 *
 * @since 20
 */

/**
 * @file job_ring.h
 *
 * @brief Declares the job_ring interfaces in C++.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 20
 */

#ifndef FFRT_JOB_RING_H
#define FFRT_JOB_RING_H
#include <functional>
#include <string>
#include <atomic>
#include "job_utils.h"
#include "cpp/task.h"

namespace ffrt {

/**
 * @struct job_ring_attr
 * @brief Defines the job ring attribute structure for controlling worker concurrency.
 *
 * This structure provides initialization and configuration for job ring attributes,
 * including QoS, threshold and busy wait time.
 *
 * @since 20
 */
struct job_ring_attr {
    /**
     * @brief Set QoS level.
     *
     * @param v QoS value.
     * @return Reference to this attribute object.
     */
    inline job_ring_attr& qos(int v)
    {
        this->qos_.store(v, std::memory_order_relaxed);
        return *this;
    }

    /**
     * @brief Set the threshold parameter for controlling the number of workers.
     *
     * @param v Threshold value.
     * @return Reference to this attribute object.
     */
    inline job_ring_attr& threshold(uint64_t v)
    {
        this->threshold_.store(v, std::memory_order_relaxed);
        return *this;
    }

    /**
     * @brief Set last worker's retry busy time (in microseconds).
     *
     * @param us Busy wait time in microseconds.
     * @return Reference to this attribute object.
     */
    inline job_ring_attr& busy(uint64_t us)
    {
        this->busy_us_.store(us, std::memory_order_relaxed);
        return *this;
    }

    /**
     * @brief Get QoS level.
     *
     * @return QoS value.
     */
    inline int qos() const
    {
        return this->qos_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get the threshold parameter for controlling the number of workers.
     *
     * @return Threshold value.
     */
    inline uint64_t threshold() const
    {
        return this->threshold_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get last worker's retry busy time (in microseconds).
     *
     * @return Busy wait time in microseconds.
     */
    inline uint64_t busy() const
    {
        return this->busy_us_.load(std::memory_order_relaxed);
    }

    job_ring_attr() = default;
    job_ring_attr(const job_ring_attr& other)
    {
        *this = other;
    }
    inline job_ring_attr& operator=(job_ring_attr const& other)
    {
        if (this != &other) {
            qos_.store(other.qos(), std::memory_order_relaxed);
            threshold_.store(other.threshold(), std::memory_order_relaxed);
            busy_us_.store(other.busy(), std::memory_order_relaxed);
        }
        return *this;
    }

private:
    std::atomic_int qos_ = ffrt::qos_user_initiated;
    std::atomic_uint64_t threshold_ = default_threshold;
    std::atomic_uint64_t busy_us_ = default_busy_us;

    static constexpr uint64_t default_threshold = 0;
    static constexpr uint64_t default_busy_us = 100;
};

/**
 * @struct job_ring
 * @brief Provide the function of submitting tasks and waiting for task completion.
 *
 * @tparam MultiProducer Indicates whether support that multiple producers submit jobs.
 * @since 20
 */
template <bool MultiProducer = true>
struct job_ring : ref_obj<job_ring<MultiProducer>> {
    using ptr = typename ref_obj<job_ring<MultiProducer>>::ptr;
    /**
    * @brief Retrieve the job_ring instance.
    *
    * @param attr Indicates job_ring attr.
    * @param depth Indicates the depth of job_ring
    * @return Returns job_ring instance.
    * @since 20
    */
    static inline auto make(const job_ring_attr& attr = {}, uint64_t depth = 1024)
    {
        return ref_obj<job_ring<MultiProducer>>::make(attr, depth);
    }

    /**
     * @brief Submits a job to the job ring.
     *
     * This function submits a job that cannot be suspended. The function is non-blocking:
     * it will not block the current thread, and the job will be asynchronously executed by a worker thread.
     *
     * @param job The job executor function closure.
     * @since 20
     */
    inline void submit(std::function<void()>&& job)
    {
        FFRT_API_LOGD("ring_submit %" X_PUBLIC "p", this);
        FFRT_API_TRACE_SCOPE("ring_submit");
        uint64_t us = 1;
        while (!q.try_push(std::forward<std::function<void()>>(job))) {
            std::this_thread::sleep_for(std::chrono::microseconds(us));
            us = us << 1;
        }
        if (num.fetch_add(1, std::memory_order_release) >= attr_.threshold() &&
            token.load(std::memory_order_relaxed) == 0) {
            flush();
        }
        sev();
    }

    /**
     * @brief Try submits a job to the job ring.
     *
     * This function submits a job that cannot be suspended. The function is non-blocking:
     * it will not block the current thread, and the job will be asynchronously executed by a worker thread.
     *
     * @param job The job executor function closure.
     * @return true when submit success, false when ring is full.
     * @since 20
     */
    inline bool try_submit(std::function<void()>&& job)
    {
        FFRT_API_LOGD("ring_try_submit %" X_PUBLIC "p", this);
        FFRT_API_TRACE_SCOPE("ring_try_submit");
        uint64_t us = 1;
        if (!q.try_push(std::forward<std::function<void()>>(job))) {
            return false;
        }
        if (num.fetch_add(1, std::memory_order_release) >= attr_.threshold() &&
            token.load(std::memory_order_relaxed) == 0) {
            flush();
        }
        sev();
        return true;
    }

    /**
     * @brief Attempt to handle existing tasks.
     *
     * If there is already a worker processing tasks, no new worker will be started.
     * the job ring will ensure that only one worker is processing the task.
     *
     * @since 20
     */
    inline void flush()
    {
        int32_t exp = 0;
        if (token.compare_exchange_strong(exp, 1, std::memory_order_acquire)) {
            this->inc_ref();
            ffrt::submit([this] {
                task_num++;
                for (;;) {
                    uint64_t count = drain();
                    if (num.fetch_sub(count, std::memory_order_release) == count) { // all job done
                        sev();
                        if (waiter.exchange(0) == 1) {
                            waiter.notify_all();
                        }
                    } else { // has job again
                        continue;
                    }
                    if (attr_.busy() > 0) {
                        FFRT_API_TRACE_SCOPE("stall");
                        auto s = clock::now();
                        while (all_done() && (clock::ns(s) < attr_.busy() * 1000)) {
                            wfe();
                        }
                        if (!all_done()) {
                            continue;
                        }
                    }

                    // start to exit
                    int32_t exp = 1;
                    if (token.compare_exchange_strong(exp, 0, std::memory_order_release)) {
                        if (!all_done()) { // have new task enqueue
                            int32_t exp = 0;
                            if (token.compare_exchange_strong(exp, 1, std::memory_order_acquire)) { // get lock again
                                continue;
                            }
                        }
                        break;
                    }
                }
                this->dec_ref();
                }, {}, {}, task_attr().qos(attr_.qos()).name("job_ring"));
        }
    }

    /**
     * @brief Waits until all submitted tasks are complete.
     *
     * This function blocks the calling thread until all submitted jobs have finished execution.
     *
     * @tparam HelpPartner If true, the current thread will also consume jobs from the worker ring.
     * @tparam BusyWaitUS If the worker ring is empty, the current thread will busy-wait for
     *                      this duration (in microseconds) before sleeping.
     *                      If a job is submitted during this time, the thread will consume it.
     * @since 20
     */
    template<bool HelpWorker = true, uint64_t BusyWaitUS = 100>
    inline void wait()
    {
        FFRT_API_LOGD("ring_wait");
        FFRT_API_TRACE_SCOPE("ring_wait");
        if (all_done()) {
            return;
        }
        if (HelpWorker) {
            int32_t exp_in = 0;
            while (token.compare_exchange_strong(exp_in, 1, std::memory_order_acquire)) {
                num.fetch_sub(drain(), std::memory_order_relaxed);
                int32_t exp_out = 1;
                if (token.compare_exchange_strong(exp_out, 0, std::memory_order_release)) {
                    if (!all_done()) { // have new task enqueue
                        continue;
                    }
                    return;
                }
            }
        } else {
            flush();
        }

        if constexpr(BusyWaitUS > 0) {
            auto s = clock::now();
            while (clock::ns(s) < BusyWaitUS * 1000) {
                if (all_done()) {
                    return;
                }
                wfe();
            }
        }
        while (!all_done()) {
            waiter = 1;
            if (!all_done()) {
                waiter.wait(1);
            }
            if (waiter.exchange(0) == 1) {
                waiter.notify_all();
            }
        }
    }

    /**
     * @brief Get the attr of the current job_ring
     *
     * @return Reference to the job_ring_attr instance of the current job_ring.
     * @since 20
     */
    inline job_ring_attr& attr()
    {
        return attr_;
    }

    /**
     * @brief Get the number of committed tasks for profiling
     *
     * @return Reference to number of committed tasks for profiling.
     * @since 20
     */
    inline uint64_t& commit_times_for_profiling()
    {
        return task_num;
    }

private:
    job_ring(const job_ring_attr& attr = {}, uint64_t depth = 1024) : attr_(attr), q(depth)
    {}

    inline bool all_done()
    {
        return num.load(std::memory_order_acquire) == 0;
    }

    static void run_one(std::function<void()>& f)
    {
        FFRT_API_TRACE_SCOPE("ring_job");
        f();
        f = nullptr;
        FFRT_API_LOGD("ring_job_done");
    }
    inline uint64_t drain()
    {
        uint64_t n = 0;
        while (q.try_pop(run_one)) {
            n++;
        }
        return n;
    }

    job_ring_attr attr_;
    alignas(detail::cacheline_size) std::atomic_int32_t token{0};
    alignas(detail::cacheline_size) std::atomic_int32_t num{0};
    uint64_t task_num{0};
    lf_queue<MultiProducer, false, std::function<void()>> q;
    atomic_wait waiter = 0;

    friend ref_obj<job_ring<MultiProducer>>;
};
}
#endif
/** @} */
