/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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
 * @brief Declares the job_partner interfaces in C++.
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
#include "common_utils.h"
#include "../task.h"

namespace ffrt {
/**
 * @struct job_partner_attr
 * @brief Defines the job_partner attribute structure.
 *
 * Provides initializing job_partner attribute settings.
 * @since 20
 */
struct job_partner_attr {
    /** QoS level */
    inline job_partner_attr& qos(qos q)
    {
        this->qos_ = q;
        return *this;
    }
    /** Max number of partner workers */
    inline job_partner_attr& max_num(uint64_t v)
    {
        this->max_num_ = v;
        return *this;
    }
    /** The waterline of jobs */
    inline job_partner_attr& scale(uint64_t v)
    {
        this->scale_ = v;
        return *this;
    }
    /** The waterline's offset */
    inline job_partner_attr& offset(uint64_t v)
    {
        this->offset_ = v;
        return *this;
    }
    /** Last partner worker's idle time */
    inline job_partner_attr& busy(uint64_t us)
    {
        this->busy_us_ = us;
        return *this;
    }
    /** The depth of job queue */
    inline job_partner_attr& queue_depth(uint64_t depth)
    {
        this->queue_depth_ = depth;
        return *this;
    }

    inline int qos() const
    {
        return this->qos_;
    }
    inline uint64_t max_num() const
    {
        return this->max_num_;
    }
    inline uint64_t scale() const
    {
        return this->scale_;
    }
    inline uint64_t offset() const
    {
        return this->offset_;
    }
    inline uint64_t busy() const
    {
        return this->busy_us_;
    }
    inline uint64_t queue_depth() const
    {
        return this->queue_depth_;
    }

private:
    int qos_ = ffrt::qos_user_initiated;
    uint64_t max_num_ = default_partner_max;
    uint64_t scale_ = default_partner_scale;
    uint64_t offset_ = default_partner_offset;
    uint64_t busy_us_ = default_partner_delay_us;
    uint64_t queue_depth_ = default_q_depth;

    static constexpr uint64_t default_partner_max = 2;
    static constexpr uint64_t default_partner_scale = 20;
    static constexpr uint64_t default_partner_offset = 0;
    static constexpr uint64_t default_partner_delay_us = 100;
    static constexpr uint64_t default_q_depth = 1024;
};

/**
 * @struct job_partner
 * @brief Provide the function of submitting tasks and waiting for task completion.
 * @tparam UsageId The user-defined job type.
 * @since 20
 */
template <int UsageId = 0>
struct job_partner : ref_obj<job_partner<UsageId>>, detail::non_copyable {
    /**
    * @brief Retrieve the job_partner instance in the current thread.
    *
    * @param attr Indicates job_partner attr.
    * @return Returns job_partner instance.
    * @since 20
    */
    static __attribute__((noinline)) auto& get_partner_of_this_thread(const job_partner_attr& attr = {})
    {
        static thread_local auto s = ref_obj<job_partner<UsageId>>::make(attr);
        return s;
    }

    /**
    * @brief Submit a job, Can be called on both master and non master threads. If the queue is full, it will retry.
    *
    * @tparam boost Indicates whether to dynamically add workers
    * @param job Indicates a job executor function closure.
    * @param stack Indicates the job's address.
    * @param stack_size Indicates the job's size.
    * @return Returns <b>1</b> job initialize failed, maybe invalid stack_size;
            returns <b>0</b> submit success.
    * @since 20
    */
    template <bool boost = true>
    inline int submit(std::function<void()>&& job, void* stack, size_t stack_size)
    {
        auto p = job_t::init(std::forward<std::function<void()>>(job), stack, stack_size);
        if (p == nullptr) {
            FFRT_API_LOGE("job initialize failed, maybe invalid stack_size");
            return 1;
        }
        FFRT_API_LOGD("submit %lu", p->id);
        p->local().partner = this;
        submit<boost>(suspendable_job_func, p);
        return 0;
    }

    /**
     * @brief Submit a job to master thread, Pause the current task after submitting the closure until the
            master thread finishes executing the closure and resumes the task. If this interface is called
            inside a non job, the closure will be executed directly, and if the queue is full, it will retry.
     *
     * @tparam boost Indicates whether to dynamically add workers
     * @param job Indicates a job executor function closure.
     * @since 20
     */
    template <bool boost = true>
    inline void submit(std::function<void()>&& job)
    {
        auto p = new non_suspendable_job_t(std::forward<std::function<void()>>(job), this);
        FFRT_API_LOGD("non-suspendable job submit: %p", p);
        submit<boost>(non_suspendable_job_func, p);
    }

    /**
     * @brief Submit a job to master thread, Pause the current task after submitting the closure until the
            master thread finishes executing the closure and resumes the task. If this interface is called
            inside a non job, the closure will be executed directly, and if the queue is full, it will retry.
     *
     * @param job Indicates a job executor function closure.
     * @since 20
     */
    static inline void submit_to_master(std::function<void()>&& job)
    {
        auto& e = job_t::env();
        auto j = e.cur;
        if (j == nullptr || j->local().partner == e.tl.token) {
            return job();
        }
        j->local().partner->submit_to_master(e, j, std::forward<std::function<void()>>(job));
    }

    /**
     * @brief Waits until all submitted tasks are complete. Can be called only on master but not non master threads.
     * @tparam help_worker Indicates if be true, current thread will consume the worker_q, else not.
     * @tparam spin_wait_us Indicates if the worker_q is empty, the current thread will wait for "spin_wait_us" time.
            If in this time, a job be submitted, the current thread will consume, else will sleep.
     * @return Returns <b>1</b> Non master thread call will wait fail and return 1;
            returns <b>0</b> wait success.
     * @since 20
     */
    template<bool help_partner = true, uint64_t master_delay_us = 100>
    inline int wait()
    {
        return _wait<help_partner, master_delay_us>();
    }

    /**
     * @brief Judge current thread is job_partner master or not.
     *
     * @return Returns <b>ture</b> is job_partner master;
            returns <b>false</b> is not job_partner master.
     * @since 20
     */
    inline bool this_thread_is_master()
    {
        return job_t::env().tl.token == this;
    }

private:
    friend ref_obj<job_partner>;
    struct fls {
        std::function<void()> master_f;
        job_partner* partner;
    };
    struct tls {
        void* token = nullptr;
    };
    using job_t = fiber<UsageId, fls, tls>;

    struct non_suspendable_job_t {
        non_suspendable_job_t(std::function<void()>&& fn, job_partner* p)
            : fn(std::forward<std::function<void()>>(fn)), partner(p) {}
        std::function<void()> fn;
        job_partner* partner;
    };

    job_partner(const job_partner_attr& attr = {})
        : name("partner<" + std::to_string(UsageId) + ">" + std::to_string(syscall(SYS_gettid))),
        attr(attr), partner_q(attr.queue_depth(), name + "_pq"), master_q(attr.queue_depth(), name + "_mq")
    {
        concurrency_name = name + "_cc#";
        partner_num_name = name + "_p#";
        job_t::env().tl.token = this;
    }

    template <bool boost>
    void submit(func_ptr f, void* p)
    {
        auto concurrency = job_num.fetch_add(1, std::memory_order_relaxed) + 1;
        FFRT_API_TRACE_INT64(concurrency_name.c_str(), concurrency);
        partner_q.template push<1>(f, p);

        auto wn = partner_num.load(std::memory_order_relaxed);
        if (boost || attr.offset()) {
            if (wn < attr.max_num() && partner_q.size() > wn * attr.scale() + attr.offset()) {
                job_partner_task();
            }
        } else {
            if (wn == 0) {
                job_partner_task();
            }
        }
    }

    template<class Env>
    void submit_to_master(Env& e, job_t* p, std::function<void()>&& job)
    {
        FFRT_API_LOGD("job %lu submit to master", (p ? p->id : -1UL));
        p->local().master_f = std::forward<std::function<void()>>(job);
        p->suspend(e, submit_to_master_suspend_func);
    }

    static bool submit_to_master_suspend_func(void* p)
    {
        auto partner = ((job_t*)p)->local().partner;
        partner->master_q.template push<0>(master_run_func, p);
        partner->notify_master();
        return true;
    }

    inline void notify_master()
    {
        if (master_wait.exchange(1) == 0) {
            master_wait.notify_one();
        }
    }

    void job_partner_task()
    {
        auto partner_n = partner_num.fetch_add(1) + 1;
        FFRT_API_TRACE_INT64(partner_num_name.c_str(), partner_n);
        FFRT_API_TRACE_SCOPE("%s add task", name.c_str());

        ref_obj<job_partner<UsageId>>::inc_ref();
        ffrt::submit([this] {
            auto wn = partner_num.load(std::memory_order_relaxed);
            if (wn < attr.max_num() && partner_q.size() > wn * attr.scale() + attr.offset()) {
                job_partner_task();
            }
_re_run_partner:
            while (partner_q.try_run());
            if (partner_num.load() == 1 && attr.busy() > 0) { // last partner delay
                FFRT_API_TRACE_SCOPE("stall");
                auto s = clock::now();
                while (clock::ns(s) < attr.busy() * 1000) {
                    if (partner_q.try_run()) {
                        goto _re_run_partner;
                    }
                    wfe();
                }
            }
            auto partner_n = partner_num.fetch_sub(1) - 1;
            FFRT_API_TRACE_INT64(partner_num_name.c_str(), partner_n);
            if (partner_q.try_run()) {
                auto partner_n = partner_num.fetch_add(1) + 1;
                FFRT_API_TRACE_INT64(partner_num_name.c_str(), partner_n);
                goto _re_run_partner;
            }
            ref_obj<job_partner<UsageId>>::dec_ref();
            }, {}, {}, task_attr().qos(attr.qos()).name(name.c_str()));
    }

    static void suspendable_job_func(void* p_)
    {
        auto p = (job_t*)p_;
        FFRT_API_LOGD("run partner job %lu", p->id);
        FFRT_API_TRACE_SCOPE("pjob%lu", p->id);
        if (p->start()) { // job done
            auto partner = p->local().partner;
            auto concurrency = partner->job_num.fetch_sub(1, std::memory_order_acquire) - 1;
            FFRT_API_TRACE_INT64(partner->concurrency_name.c_str(), concurrency);
            if (concurrency == 0) {
                partner->notify_master();
            }
            p->destroy();
        }
    }

    static void non_suspendable_job_func(void* p_)
    {
        auto p = (non_suspendable_job_t*)p_;
        FFRT_API_LOGD("run non-suspendable job %p", p);
        FFRT_API_TRACE_SCOPE("nsjob");
        (p->fn)();
        auto partner = p->partner;
        auto concurrency = partner->job_num.fetch_sub(1, std::memory_order_acquire) - 1;
        FFRT_API_TRACE_INT64(partner->concurrency_name.c_str(), concurrency);
        if (concurrency == 0) {
            partner->notify_master();
        }
        delete p;
    }

    static void master_run_func(void* p_)
    {
        auto p = (job_t*)p_;
        {
            FFRT_API_LOGD("run master job %lu", p->id);
            FFRT_API_TRACE_SCOPE("mjob%lu", p->id);
            p->local().master_f();
            p->local().master_f = nullptr;
        }
        p->local().partner->partner_q.template push<1>(suspendable_job_func, p);
    }

    template<bool help_partner = true, uint64_t master_delay_us = 100>
    int _wait()
    {
        if (!this_thread_is_master()) {
            FFRT_API_LOGE("wait only can be called on master thread");
            return 1;
        }
        FFRT_API_TRACE_SCOPE("%s wait on master", name.c_str());
        FFRT_API_LOGD("wait on master");

        for (;;) {
_begin_consume_master_job:
            int idx = 0;
            while (master_q.try_run()) {
                if (((++idx & 0xF) == 0) && partner_num.load(std::memory_order_relaxed) == 0) {
                    job_partner_task();
                }
            }

            auto concurrency = job_num.load();
            auto wn = partner_num.load(std::memory_order_relaxed);
            if (wn < attr.max_num() && partner_q.size() > wn * attr.scale() + attr.offset()) {
                job_partner_task();
            }
            if (help_partner && partner_q.try_run()) {
                goto _begin_consume_master_job;
            }
            if (concurrency == 0) {
                break;
            } else {
                auto s = clock::now();
                while (!help_partner && master_delay_us > 0 && clock::ns(s) < master_delay_us * 1000) {
                    if (master_q.try_run()) {
                        goto _begin_consume_master_job;
                    }
                    wfe();
                }
                master_wait.wait(0);
                master_wait = 0;
            }
        }

        FFRT_API_LOGD("wait success");
        return 0;
    }

    std::string name;
    std::string concurrency_name;
    std::string partner_num_name;
    job_partner_attr attr;
    alignas(detail::cacheline_size) std::atomic_uint64_t partner_num{0};
    alignas(detail::cacheline_size) std::atomic_uint64_t job_num{0};

    runnable_queue<mpmc_queue> partner_q;
    runnable_queue<mpmc_queue> master_q;
    atomic_wait master_wait = 0;
};
}
#endif