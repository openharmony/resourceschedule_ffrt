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
 * @file job_partner.h
 *
 * @brief Declares the job partner interfaces in C++.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 20
 */

#ifndef FFRT_JOB_PARTNER_H
#define FFRT_JOB_PARTNER_H

#include <functional>
#include <string>
#include <atomic>
#include "job_utils.h"
#include "cpp/task.h"

namespace ffrt {

/**
 * @struct job_partner_attr
 * @brief Defines the job partner attribute structure for controlling worker concurrency.
 *
 * This structure provides initialization and configuration for job partner attributes,
 * including QoS, maximum worker number, ratio, threshold and busy wait time.
 *
 * The relationship between job number and partner number is illustrated as follows:
 * @verbatim
 * partner_num
 *     ^
 *     |         
 *     |------------------ max_parallelism
 *     |       /   
 *     |      / ratio
 *     |     /     
 *     +------------------------------> job_num
 *       threshold
 * @endverbatim
 *
 * - The vertical axis is partner_num, and the horizontal axis is job_num.
 * - Threshold: When job_num is less than threshold, partner_num is 0.
 * - Ratio control: When job_num is between threshold and "max * ratio + threshold",
 *   partner_num is calculated as "round((job_num - threshold) / ratio)".
 * - Maximum value: When job_num is greater than "max * ratio + threshold", partner_num is the maximum value.
 *
 * @since 20
 */
struct job_partner_attr {
    /**
     * @brief Set QoS level.
     *
     * @param v QoS value.
     * @return Reference to this attribute object.
     */
    inline job_partner_attr& qos(int v)
    {
        this->qos_.store(v, std::memory_order_relaxed);
        return *this;
    }

    /**
     * @brief Set max number of partner workers.
     *
     * @param v Maximum number of workers.
     * @return Reference to this attribute object.
     */
    inline job_partner_attr& max_parallelism(uint64_t v)
    {
        this->max_parallelism_.store(v, std::memory_order_relaxed);
        return *this;
    }

    /**
     * @brief Set the ratio parameter for controlling the number of workers.
     *
     * @param v Ratio value.
     * @return Reference to this attribute object.
     */
    inline job_partner_attr& ratio(uint64_t v)
    {
        this->ratio_.store(v, std::memory_order_relaxed);
        return *this;
    }

    /**
     * @brief Set the threshold parameter for controlling the number of workers.
     *
     * @param v Threshold value.
     * @return Reference to this attribute object.
     */
    inline job_partner_attr& threshold(uint64_t v)
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
    inline job_partner_attr& busy(uint64_t us)
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
     * @brief Get max number of partner workers.
     *
     * @return Maximum number of workers.
     */
    inline uint64_t max_parallelism() const
    {
        return this->max_parallelism_.load(std::memory_order_relaxed);
    }

    /**
     * @brief Get the ratio parameter for controlling the number of workers.
     *
     * @return Ratio value.
     */
    inline uint64_t ratio() const
    {
        return this->ratio_.load(std::memory_order_relaxed);
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

    job_partner_attr() = default;
    job_partner_attr(const job_partner_attr& other)
    {
        *this = other;
    }
    inline job_partner_attr& operator=(job_partner_attr const& other)
    {
        if (this != &other) {
            qos_.store(other.qos(), std::memory_order_relaxed);
            max_parallelism_.store(other.max_parallelism(), std::memory_order_relaxed);
            ratio_.store(other.ratio(), std::memory_order_relaxed);
            threshold_.store(other.threshold(), std::memory_order_relaxed);
            busy_us_.store(other.busy(), std::memory_order_relaxed);
        }
        return *this;
    }
private:
    std::atomic_int qos_ = static_cast<int>(ffrt::qos_user_initiated);             ///< QoS level for the job partner.
    std::atomic_uint64_t max_parallelism_ = default_max_parallelism;         ///< Maximum number of partner workers.
    std::atomic_uint64_t ratio_ = default_partner_ratio;         ///< Ratio for scaling the number of workers.
    std::atomic_uint64_t threshold_ = default_partner_threshold; ///< Threshold for scaling the number of workers.
    std::atomic_uint64_t busy_us_ = default_partner_busy_us;    ///< Busy wait for the last worker before exit.

    static constexpr uint64_t default_max_parallelism = 2;        ///< Default max number of partner workers.
    static constexpr uint64_t default_partner_ratio = 20;     ///< Default ratio for worker scaling.
    static constexpr uint64_t default_partner_threshold = 0;  ///< Default threshold for worker scaling.
    static constexpr uint64_t default_partner_busy_us = 100; ///< Default busy wait time (us) for last worker.
};

/**
 * @struct job_partner
 * @brief Provide the function of submitting tasks and waiting for task completion.
 *
 * @tparam UsageId The user-defined job type.
 * @since 20
 */
template <uint64_t UsageId = 0>
struct job_partner : ref_obj<job_partner<UsageId>>, detail::non_copyable {
    using ptr = typename ref_obj<job_partner<UsageId>>::ptr;
    /**
    * @brief Retrieve the job_partner instance in the main thread.
    *
    * @param attr Indicates job_partner attr.
    * @return Returns job_partner instance.
    * @since 20
    */
    static __attribute__((noinline)) auto& get_main_partner()
    {
        static auto s = ref_obj<job_partner<UsageId>>::make(getpid());
        return s;
    }

    /**
    * @brief Retrieve the job_partner instance in the main thread and reset attribute.
    *
    * @param attr Indicates job_partner attr.
    * @return Returns job_partner instance.
    * @since 20
    */
    static inline auto& get_main_partner(const job_partner_attr& attr)
    {
        auto& s = get_main_partner();
        s->attr() = attr;
        return s;
    }

   /*
    * @brief Retrieves the job_partner instance in the current thread.
    *
    * @param attr Job partner attributes.
    * @return Reference to the job_partner instance.
    * @since 20
    */
    static __attribute__((noinline)) auto& get_partner_of_this_thread()
    {
        static thread_local auto s = ref_obj<job_partner<UsageId>>::make(tid());
        return s;
    }

   /*
    * @brief Retrieves the job_partner instance in the current thread and reset attribute.
    *
    * @param attr Job partner attributes.
    * @return Reference to the job_partner instance.
    * @since 20
    */
    static inline auto& get_partner_of_this_thread(const job_partner_attr& attr)
    {
        auto& s = get_partner_of_this_thread();
        s->attr() = attr;
        return s;
    }

#ifdef _ffrt_has_fiber_feature
    /**
     * @brief Submits a suspendable job to the partner thread (blocking).
     *
     * This function submits a job that can be suspended and resumed, using the specified stack and stack size.
     * The function is blocking: it will block the current thread until the job is successfully executed.
     * It can be called from both master and non-master threads. If the queue is full, it will retry until successful.
     *
     * @tparam Boost Indicates whether to dynamically add workers.
     * @param suspendable_job The job executor function closure.
     * @param stack Pointer to the stack memory for the job.
     * @param stack_size Size of the stack memory.
     * @param on_done The callback function to be executed after the job is done.
     * @return Returns 1 if job initialization failed (e.g., invalid stack_size); 0 if submission succeeded.
     * @since 20
     */
    template <bool Boost = true>
    inline int submit(std::function<void()>&& suspendable_job, void* stack, size_t stack_size,
                      void (*on_done)(void*) = nullptr)
    {
        auto p = job_t::init(std::forward<std::function<void()>>(suspendable_job), stack, stack_size);
        if (p == nullptr) {
            FFRT_API_LOGE("job initialize failed, maybe invalid stack_size");
            return 1;
        }
        FFRT_API_LOGD("submit %lu", p->id());
        p->local().partner = this;
        p->local().on_done = on_done;
        submit<Boost>(suspendable_job_func, p);
        return 0;
    }

    /**
     * @brief Submits a job to the master thread and suspends the current task until completion.
     *
     * This function submits a job to the master thread. The current task will be paused after submitting the closure,
     * and will resume only after the master thread finishes executing the closure. If called outside a job context,
     * the closure will be executed directly. If the queue is full, it will retry until successful.
     *
     * @param job The job executor function closure.
     * @since 20
     */
    static inline void submit_to_master(const std::function<void()>& job)
    {
        auto& e = job_t::env();
        auto j = e.cur;
        if (j == nullptr || j->local().partner->token == e.tl.thread_id) {
            return job();
        }
        j->local().partner->submit_to_master(e, j, job);
    }

    /**
     * @brief Judge whether the current thread is the job_partner master.
     *
     * @return true if the current thread is the job_partner master; false otherwise.
     * @since 20
     */
    inline bool this_thread_is_master()
    {
        return job_t::env().tl.thread_id == this->token;
    }
#endif

    /**
     * @brief Submits a non-suspendable job to the partner thread (non-blocking).
     *
     * This function submits a job that cannot be suspended. The function is non-blocking:
     * it will not block the current thread, and the job will be asynchronously executed by a partner worker thread.
     *
     * @tparam Boost Indicates whether to dynamically add workers.
     * @param non_suspendable_job The job executor function closure.
     * @since 20
     */
    template <bool Boost = true>
    inline void submit(std::function<void()>&& non_suspendable_job)
    {
        auto p = new non_suspendable_job_t(std::forward<std::function<void()>>(non_suspendable_job), this);
        FFRT_API_LOGD("non-suspendable job submit: %p", p);
        submit<Boost>(non_suspendable_job_func, p);
    }

    /**
     * @brief Submits a non-suspendable job to the partner thread (non-blocking) and convert the job as a future
     *        what's you want.
     *
     * This function submits a job that cannot be suspended and the job can be converted to a future what's you want.
     * The function is non-blocking:
     * it will not block the current thread, and the job will be asynchronously executed by a partner worker thread.
     *
     * @tparam Boost Indicates whether to dynamically add workers.
     * @tparam Function Indicates the type of the job function.
     * @param non_suspendable_job The job executor function closure.
	 * @param r The job executor function closure.
     * @since 20
     */
    template <bool Boost = true, class Function>
    inline auto submit_p(Function&& non_suspendable_job)
    {
        using R = std::invoke_result_t<Function>;
        auto r = make_job_promise<R>(std::forward<Function>(non_suspendable_job));
        FFRT_API_LOGD("non-suspendable future job submit: %p", (void*)r);
        r->owner = this;
        r->inc_ref();
        submit<Boost>(non_suspendable_future_job_func<R>, r);
        return std::move(r);
    }

    /**
     * @brief Waits until all submitted tasks are complete.
     *
     * This function blocks the calling thread until all submitted jobs have finished execution.
     *
     * @tparam HelpPartner If true, the current thread will also consume jobs from the worker queue.
     * @tparam BusyWaitUS If the worker queue is empty, the current thread will busy-wait for
     *                      this duration (in microseconds) before sleeping.
     *                      If a job is submitted during this time, the thread will consume it.
     * @since 20
     */
    template<bool HelpPartner = true, uint64_t BusyWaitUS = 100>
    void wait()
    {
        if (all_done()) {
            return;
        }
#ifdef _ffrt_has_fiber_feature
        bool is_master = this_thread_is_master();
#else
        bool is_master = false;
#endif
        FFRT_API_TRACE_SCOPE("%s wait on master", name.c_str());
        FFRT_API_LOGD("wait on master");

        for (;;) {
            if (is_master) {
                int idx = 0;
                while (master_q->try_run()) {
                    if ((++idx & 0xF) == 0) {
                        trigger_partner<true>();
                    }
                }
            }
            trigger_partner<true>();

            if (HelpPartner && partner_q->try_run()) {
                continue;
            }
            if (all_done()) {
                break;
            } else {
                if (BusyWaitUS > 0) {
                    auto s = clock::now();
                    while (!has_job<HelpPartner>(is_master) && clock::ns(s) < BusyWaitUS * 1000) {
                        wfe();
                    }
                }
                master_wait = 1;
                if (!all_done() && !has_job<HelpPartner>(is_master)) {
                    master_wait.wait(1);
                }
                if (master_wait.exchange(0) == 1) {
                    master_wait.notify_all();
                }
            }
        }

        FFRT_API_LOGD("wait success");
    }

    /**
     * @brief Get the attr of the current job_partner
     *
     * @return Reference to the job_partner_attr instance of the current job_partner.
     * @since 20
     */
    inline job_partner_attr& attr()
    {
        return attr_;
    }

    /**
     * @brief Attempt to add a job_partner to handle existing tasks.
     *
     * If the number of workers is satisfying the requirements, no new threads
     * will be added to handle the jobs.
     *
     * @since 20
     */
    inline void flush()
    {
        trigger_partner<true, true>();
    }

    /**
     * @brief Attempt to steal the job at the first of the queue.
     *
     * @tparam StealPartnerJob If true will attempt to steal the job in the partner queue,
                                else in the master queue.
     * @return Returns 1 if steal the job successded 0 if steal failed such as before stealling, the queue is empty.
     * @since 20
     */
    template<bool StealPartnerJob = true>
    inline bool try_steal_one_job()
    {
        if constexpr(StealPartnerJob) {
            return partner_q->try_run();
        } else {
            return master_q->try_run();
        }
    }

    /**
     * @brief when call this interface, must ensure that the queue is empty,
                such as before submitting a task or after waiting.
     *
     * @param partner_queue_depth Indicates the depth of the partner queue.
     * @param master_queue_depth Indicates the depth of the master queue.
     * @since 20
     */
    inline void reset_queue(uint64_t partner_queue_depth = 1024, uint64_t master_queue_depth = 1024)
    {
        partner_q = std::make_unique<runnable_queue<mpmc_queue>>(partner_queue_depth, name + "_pq");
        master_q = std::make_unique<runnable_queue<mpmc_queue>>(master_queue_depth, name + "_mq");
    }

private:
    friend ref_obj<job_partner>; ///< Allows ref_obj to access private members for reference counting.

    /**
     * @brief Structure representing a non-suspendable job.
     */
    struct non_suspendable_job_t {
        /**
         * @brief Constructs a non_suspendable_job_t object.
         *
         * @param fn Function to execute.
         * @param p Pointer to the associated job_partner.
         */
        non_suspendable_job_t(std::function<void()>&& fn, job_partner* p)
            : fn(std::forward<std::function<void()>>(fn)), partner(p) {}

        std::function<void()> fn; ///< Function to execute.
        job_partner* partner;     ///< Pointer to the associated job_partner.
    };

    /**
     * @brief Constructs a job_partner object with the given attributes.
     *
     * @param attr Job partner attributes.
     */
    job_partner(uint64_t thread_id = tid())
        : name("partner<" + std::to_string(UsageId) + ">" + std::to_string(thread_id))
    {
        reset_queue();
        concurrency_name = name + "_cc#";
        partner_num_name = name + "_p#";
        token = thread_id;
    }

    static uint64_t tid()
    {
        static thread_local uint64_t tid = syscall(SYS_gettid);
        return tid;
    }

	/**
     * @brief Submits a job to the partner queue.
     *
     * @tparam boost Indicates whether to dynamically add workers.
     * @param f Function pointer for the job.
     * @param p Pointer to the job data.
     */
    template <bool Boost>
    inline void submit(void(*f)(void*), void* p)
    {
        auto concurrency = job_num.fetch_add(1, std::memory_order_relaxed) + 1;
        (void)(concurrency);
        FFRT_API_TRACE_INT64(concurrency_name.c_str(), concurrency);
        partner_q->template push<1>(f, p);
        sev();
        trigger_partner<Boost>();
    }

#ifdef _ffrt_has_fiber_feature
    /**
     * @brief Fiber-local storage structure for master function and partner pointer.
     */
    struct fls {
        const std::function<void()>* master_f; ///< Function to be executed by the master.
        job_partner* partner;           ///< Pointer to the associated job_partner instance.
        void(*on_done)(void*);
    };

    /**
     * @brief Thread-local storage structure for token identification.
     */
    struct tls {
        uint64_t thread_id = tid(); ///< thread_id used to identify the current job_partner instance.
    };

    /**
     * @brief Alias for the fiber type used by this job_partner.
     */
    using job_t = fiber<UsageId, fls, tls>;

    /**
     * @brief Submits a job to the master queue and suspends the current fiber.
     *
     * @tparam Env Environment type.
     * @param e Reference to the environment.
     * @param p Pointer to the job fiber.
     * @param job Function to execute.
     */
    template<class Env>
    inline void submit_to_master(Env& e, job_t* p, const std::function<void()>& job)
    {
        FFRT_API_LOGD("job %lu submit to master", (p ? p->id() : -1UL));
        p->local().master_f = &job;
        p->suspend(e, submit_to_master_suspend_func);
    }

    /**
     * @brief Suspend function used when submitting to master.
     *
     * @param p Pointer to the job fiber.
     * @return True if suspension is successful.
     */
    static bool submit_to_master_suspend_func(void* p)
    {
        auto partner = (static_cast<job_t*>(p))->local().partner;
        partner->master_q->template push<0>(master_run_func, p);
        sev();
        partner->notify_master();
        return true;
    }

    /**
     * @brief Function executed by the master to run a job.
     *
     * @param p_ Pointer to the job fiber.
     */
    static void master_run_func(void* p_)
    {
        auto p = static_cast<job_t*>(p_);
        {
            FFRT_API_LOGD("run master job %lu", p->id());
            FFRT_API_TRACE_SCOPE("mjob%lu", p->id());
            (*(p->local().master_f))();
        }
        p->local().partner->partner_q->template push<1>(suspendable_job_func, p);
        sev();
    }

    /**
     * @brief Function executed by a suspendable job.
     *
     * @param p_ Pointer to the job fiber.
     */
    static void suspendable_job_func(void* p_)
    {
        auto p = (static_cast<job_t*>(p_));
        FFRT_API_LOGD("run partner job %lu", p->id());
        FFRT_API_TRACE_SCOPE("pjob%lu", p->id());
        if (p->start()) { // job done
            p->local().partner->done_one();
            p->destroy();
            auto cb = p->local().on_done;
            if (cb) cb(p);
        }
    }
#endif

    inline void done_one()
    {
        auto concurrency = job_num.fetch_sub(1, std::memory_order_release) - 1;
        FFRT_API_TRACE_INT64(concurrency_name.c_str(), concurrency);
        if (concurrency == 0) {
            sev();
            notify_master();
        }
    }

    /**
     * @brief Notifies the master thread to wake up if waiting.
     */
    inline void notify_master()
    {
        if (master_wait.exchange(0) == 1) {
            master_wait.notify_all();
        }
    }

    template <bool Boost, bool IgnoreThreshold = false>
    inline void trigger_partner()
    {
        if (try_add_partner<Boost>()) {
            add_partner();
        }
    }

    template <bool Boost, bool IgnoreThreshold = false>
    inline bool try_add_partner()
    {
        auto th = IgnoreThreshold ? 0 : attr_.threshold();
        auto wn = partner_num.load(std::memory_order_acquire);
        for (;;) {
            if ((Boost && wn < attr_.max_parallelism() && partner_q->size() > wn * attr_.ratio() + th) ||
                (!Boost && wn == 0 && partner_q->size() > th)) {
                if (partner_num.compare_exchange_weak(wn, wn + 1, std::memory_order_relaxed)) {
                    FFRT_API_TRACE_INT64(partner_num_name.c_str(), wn + 1);
                    return true;
                }
            } else {
                return false;
            }
        }
        return false;
    }

    /**
     * @brief Launches a new partner worker task.
     */
    void add_partner()
    {
        job_partner<UsageId>::inc_ref();
        ffrt::submit([this] {
            for (;;) {
                trigger_partner<true>();

                while (partner_q->try_run());

                auto partner_n = partner_num.fetch_sub(1) - 1;
                FFRT_API_TRACE_INT64(partner_num_name.c_str(), partner_n);
                if (partner_n == 0 && attr_.busy() > 0 &&
                    partner_num.compare_exchange_strong(partner_n, partner_n + 1)) { // last partner delay
                    FFRT_API_TRACE_SCOPE("stall");
                    auto s = clock::now();
                    while (partner_q->size() == 0 && clock::ns(s) < attr_.busy() * 1000) {
                        wfe();
                    }
                    if (partner_q->size() > 0) { // has job again
                        continue;
                    }
                    partner_num.fetch_sub(1);
                }
                if (partner_q->size() > 0) { // has job again
                    if (try_add_partner<true>()) { // run continue
                        continue;
                    }
                }
                break;
            }
            job_partner<UsageId>::dec_ref();
            }, {}, {}, task_attr().qos(attr_.qos()).name(name.c_str()));
    }

    /**
     * @brief Judge the current thread has job to do or not.
     *
     * @tparam HelpPartner if master thread calls the method, HelpPartner indicates
                            whether it will help partner thread handle jobs
     * @param is_master Indicates is master thread or not.
     */
    template<bool HelpPartner>
    inline bool has_job(bool is_master)
    {
        return (is_master && master_q->size() > 0) || (HelpPartner && partner_q->size() > 0);
    }

    /**
     * @brief Judge whether the jobs in partner queue and master queue are all done.
     */
    inline bool all_done()
    {
        return job_num.load(std::memory_order_acquire) == 0;
    }

    /**
     * @brief Function executed by a non-suspendable job.
     *
     * @param p_ Pointer to the non_suspendable_job_t object.
     */
    static void non_suspendable_job_func(void* p_)
    {
        auto p = static_cast<non_suspendable_job_t*>(p_);
        FFRT_API_LOGD("run non-suspendable job %p", p);
        FFRT_API_TRACE_SCOPE("nsjob");
        (p->fn)();
        p->partner->done_one();
        delete p;
    }

    template <class R>
    static void non_suspendable_future_job_func(void* p)
    {
        FFRT_API_LOGD("run non-suspendable future job %p", p);
        FFRT_API_TRACE_SCOPE("nsfjob");
        auto f = (job_promise_obj<R>*)p;
        f->try_execute();
        auto partner = (job_partner<UsageId>*)(f->owner);
        partner->done_one();
        f->dec_ref();
    }

    std::string name;             ///< Name of the job_partner instance.
    std::string concurrency_name; ///< Name used for concurrency tracing.
    std::string partner_num_name; ///< Name used for partner number tracing.
    job_partner_attr attr_;        ///< Attributes for configuring the job_partner.
    uint64_t token;
    alignas(detail::cacheline_size) std::atomic_uint64_t partner_num{0}; ///< Number of active partner workers.
    alignas(detail::cacheline_size) std::atomic_uint64_t job_num{0};     ///< Number of active jobs.

    std::unique_ptr<runnable_queue<mpmc_queue>> partner_q; ///< Runnable queue for partner jobs.
    std::unique_ptr<runnable_queue<mpmc_queue>> master_q;  ///< Runnable queue for master jobs.
    atomic_wait master_wait = 0;          ///< Synchronization primitive for master waiting.
};

} // namespace ffrt

#endif // FFRT_JOB_PARTNER_H
/** @} */
