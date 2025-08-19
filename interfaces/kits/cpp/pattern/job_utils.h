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
 * @file job_utils.h
 *
 * @brief Declares utilities for job scheduling, synchronization, and fiber management in FFRT.
 *
 * @library libffrt.z.so
 * @kit FunctionFlowRuntimeKit
 * @syscap SystemCapability.Resourceschedule.Ffrt.Core
 * @since 20
 */

#ifndef FFRT_JOB_UTILS_H
#define FFRT_JOB_UTILS_H

#include <cstdint>
#include <climits>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <functional>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#if __has_include("c/fiber.h")
#include "c/fiber.h"
#define _ffrt_has_fiber_feature
#endif

#ifndef FFRT_API_LOGE
#define FFRT_API_LOGE(fmt, ...)
#endif
#ifndef FFRT_API_LOGD
#define FFRT_API_LOGD(fmt, ...)
#endif
#ifndef FFRT_API_TRACE_INT64
#define FFRT_API_TRACE_INT64(name, value)
#endif
#ifndef FFRT_API_TRACE_SCOPE
#define FFRT_API_TRACE_SCOPE(fmt, ...)
#endif

namespace ffrt {

/**
 * @namespace ffrt::detail
 * @brief Internal implementation details for FFRT utilities.
 */
namespace detail {
    /**
     * @brief Cache line size constant.
     */
    static constexpr uint64_t cacheline_size = 64;

    /**
     * @brief Non-copyable base class. Inherit from this to prevent copy and assignment.
     */
    struct non_copyable {
    protected:
        non_copyable() = default;
        ~non_copyable() = default;
        non_copyable(const non_copyable&) = delete;
        non_copyable& operator=(const non_copyable&) = delete;
    };
} // ffrt::detail

/**
 * @brief Wait for event (WFE) instruction for ARM architectures.
 */
static inline void wfe()
{
#if (defined __aarch64__ || defined __arm__)
    __asm__ volatile("wfe" : : : "memory");
#endif
}

static inline void sev()
{
#if (defined __aarch64__ || defined __arm__)
    __asm__ volatile ("sev" : : : "memory");
#endif
}

/**
 * @brief Aligns a value to the next power of two.
 *
 * @param x Input value.
 * @return The next power of two greater than or equal to x.
 */
static inline constexpr uint64_t align2n(uint64_t x)
{
    uint64_t i = 1;
    uint64_t t = x;
    while (x >>= 1) {
        i <<= 1;
    }
    return (i < t) ? (i << 1) : i;
}

/**
 * @struct futex
 * @brief Futex-based synchronization primitives.
 * @details Provides wait and wake operations using Linux futex syscall.
 */
struct futex {
    /**
     * @brief Waits on a futex address until its value changes.
     *
     * @param uaddr Address to wait on.
     * @param val Expected value.
     */
    static inline void wait(int* uaddr, int val)
    {
        FFRT_API_LOGD("futex wait in %p", uaddr);
        int r = call(uaddr, FUTEX_WAIT_PRIVATE, val, nullptr, 0);
        (void)(r);
        FFRT_API_LOGD("futex wait %p ret %d", uaddr, r);
    }

    /**
     * @brief Wakes up threads waiting on a futex address.
     *
     * @param uaddr Address to wake.
     * @param num Number of threads to wake.
     */
    static inline void wake(int* uaddr, int num)
    {
        int r = call(uaddr, FUTEX_WAKE_PRIVATE, num, nullptr, 0);
        (void)(r);
        FFRT_API_LOGD("futex wake %p ret %d", uaddr, r);
    }

private:
    /**
     * @brief Internal futex syscall wrapper.
     */
    static inline int call(int* uaddr, int op, int val, const struct timespec* timeout, int bitset)
    {
        return syscall(SYS_futex, uaddr, op, val, timeout, NULL, bitset);
    }
};

/**
 * @struct atomic_wait
 * @brief Atomic integer with futex-based wait/notify.
 * @details Extends std::atomic<int> to support futex-based waiting and notification.
 */
struct atomic_wait : std::atomic<int> {
    using std::atomic<int>::atomic;
    using std::atomic<int>::operator=;

    /**
     * @brief Waits until the atomic value changes from val.
     *
     * @param val Expected value.
     */
    inline void wait(int val)
    {
        futex::wait(reinterpret_cast<int*>(this), val);
    }

    /**
     * @brief Notifies one waiting thread.
     */
    inline auto notify_one()
    {
        futex::wake(reinterpret_cast<int*>(this), 1);
    }

    /**
     * @brief Notifies all waiting threads.
     */
    inline void notify_all()
    {
        futex::wake(reinterpret_cast<int*>(this), INT_MAX);
    }
};

/**
 * @struct ref_obj
 * @brief Reference-counted object base class.
 *
 * @tparam T Object type.
 */
template <class T>
struct ref_obj {
    /**
     * @struct ptr
     * @brief Smart pointer for reference-counted objects.
     *
     * Provides automatic reference counting and resource management for objects derived from ref_obj.
     */
    struct ptr {
        using type = T;
        /**
         * @brief Default constructor. Initializes as a null pointer.
         */
        ptr() {}

        /**
         * @brief Constructs from a raw pointer, taking ownership.
         *
         * @param p Raw pointer to the managed object.
         */
        ptr(void* p) : p(static_cast<T*>(p)) {}

        /**
         * @brief Destructor. Decreases the reference count and deletes the object if necessary.
         */
        ~ptr()
        {
            reset();
        }

        /**
         * @brief Copy constructor. Increases the reference count.
         *
         * @param h The smart pointer to copy from.
         */
        inline ptr(ptr const& h)
        {
            *this = h;
        }

        /**
         * @brief Copy assignment operator. Increases the reference count.
         *
         * @param h The smart pointer to assign from.
         * @return Reference to this pointer.
         */
        inline ptr& operator=(ptr const& h)
        {
            if (this != &h) {
                if (p) {
                    p->dec_ref();
                }
                p = h.p;
                if (p) {
                    p->inc_ref();
                }
            }
            return *this;
        }

        /**
         * @brief Move constructor. Transfers ownership without increasing the reference count.
         *
         * @param h The smart pointer to move from.
         */
        inline ptr(ptr&& h)
        {
            *this = std::move(h);
        }

        /**
         * @brief Move assignment operator. Transfers ownership without increasing the reference count.
         *
         * @param h The smart pointer to move from.
         * @return Reference to this pointer.
         */
        inline ptr& operator=(ptr&& h)
        {
            if (this != &h) {
                if (p) {
                    p->dec_ref();
                }
                p = h.p;
                h.p = nullptr;
            }
            return *this;
        }

        /**
         * @brief Returns the raw pointer to the managed object.
         *
         * @return Raw pointer to the object.
         */
        constexpr inline T* get()
        {
            return p;
        }

        /**
         * @brief Returns the raw pointer to the managed object (const version).
         *
         * @return Const raw pointer to the object.
         */
        constexpr inline const T* get() const
        {
            return p;
        }

        /**
         * @brief Arrow operator for member access.
         *
         * @return Raw pointer to the object.
         */
        constexpr inline T* operator -> () const
        {
            return p;
        }

        /**
         * @brief Conversion operator to void pointer.
         *
         * @return Raw pointer as void*.
         */
        inline operator void* () const
        {
            return p;
        }

        /**
         * @brief Releases the owned object and decreases the reference count.
         */
        inline void reset()
        {
            if (p) {
                p->dec_ref();
                p = nullptr;
            }
        }

    private:
        T* p = nullptr; ///< Raw pointer to the managed reference-counted object.
    };

    /**
     * @brief Creates a new reference-counted object.
     */
    template<class... Args>
    static inline ptr make(Args&& ... args)
    {
        auto p = new T(std::forward<Args>(args)...);
        return ptr(p);
    }

    /**
     * @brief Returns a singleton instance.
     */
    template<class... Args>
    static ptr& singleton(Args&& ... args)
    {
        static ptr s = make(std::forward<Args>(args)...);
        return s;
    }

    /**
     * @brief Increments the reference count.
     */
    inline void inc_ref()
    {
        ref.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Decrements the reference count and deletes the object if zero.
     */
    inline void dec_ref()
    {
        if (ref.fetch_sub(1, std::memory_order_release) == 1) {
            std::atomic_thread_fence(std::memory_order_acquire);
            delete static_cast<T*>(this);
        }
    }

private:
    std::atomic_uint64_t ref{1}; ///< Atomic reference counter for managing the lifetime of the object.
};

/**
 * @struct mpmc_queue
 * @brief Lock-free multi-producer multi-consumer queue.
 *
 * @tparam T Element type.
 */
template <typename T>
struct mpmc_queue : detail::non_copyable {
    /**
     * @brief Constructs a queue with the given capacity.
     *
     * @param cap Capacity of the queue.
     */
    mpmc_queue(uint64_t cap) : capacity(align2n(cap)), mask(capacity - 1)
    {
        uint64_t us = 1;
        q = nullptr;
        for (;;) {
            if (std::is_pod_v<Item>) {
                q = static_cast<Item*>(malloc(capacity * sizeof(Item)));
            } else {
                q = new (std::nothrow) Item[capacity];
            }
            if (q) break;
            std::this_thread::sleep_for(std::chrono::microseconds(us));
            us = us << 1;
        }

        for (size_t i = 0; i < capacity; ++i) {
            q[i].iwrite_exp.store(i, std::memory_order_relaxed);
            q[i].iread_exp.store(-1, std::memory_order_relaxed);
        }

        iwrite_.store(0, std::memory_order_relaxed);
        iread_.store(0, std::memory_order_relaxed);
    }

    /**
     * @brief Destructor.
     */
    ~mpmc_queue()
    {
        if (std::is_pod_v<Item>) {
            free(q);
        } else {
            delete [] q;
        }
    }

    /**
     * @brief Returns the current size of the queue.
     */
    inline uint64_t size() const
    {
        auto head = iread_.load(std::memory_order_relaxed);
        return iwrite_.load(std::memory_order_relaxed) - head;
    }

    /**
     * @brief Attempts to push an element into the queue.
     *
     * @param data Element to push.
     * @return True if successful, false otherwise.
     */
    template <class Data>
    inline bool try_push(Data&& data)
    {
        Item* i;
        auto iwrite = iwrite_.load(std::memory_order_relaxed);
        for (;;) {
            i = &q[iwrite & mask];
            if (i->iwrite_exp.load(std::memory_order_relaxed) != iwrite) {
                return false;
            }
            if ((iwrite_.compare_exchange_weak(iwrite, iwrite + 1, std::memory_order_relaxed))) {
                break;
            }
        }
        i->data = std::forward<Data>(data);
        i->iread_exp.store(iwrite, std::memory_order_release);
        return true;
    }

    /**
     * @brief Attempts to pop an element from the queue.
     *
     * @param result Output parameter for the popped element.
     * @return True if successful, false otherwise.
     */
    template <class R>
    inline bool try_pop(R& result)
    {
        Item* i;
        auto iread = iread_.load(std::memory_order_relaxed);
        for (;;) {
            i = &q[iread & mask];
            if (i->iread_exp.load(std::memory_order_relaxed) != iread) {
                return false;
            }
            if (iread_.compare_exchange_weak(iread, iread + 1, std::memory_order_relaxed)) {
                break;
            }
        }
        if constexpr(std::is_same_v<R, T>) {
            result = std::move(i->data);
        } else {
            result(i->data);
        }
        i->iwrite_exp.store(iread + capacity, std::memory_order_release);
        return true;
    }

private:
    uint64_t capacity; ///< Capacity of the queue (must be a power of two).
    uint64_t mask;     ///< Bitmask used for efficient index calculation (capacity - 1).

    /**
     * @brief Internal structure representing a queue slot.
     */
    struct Item {
        T data;                           ///< Data stored in the queue slot.
        std::atomic<uint64_t> iwrite_exp; ///< Expected write index after a read operation.
        std::atomic<uint64_t> iread_exp;  ///< Expected read index after a write operation
    };

    alignas(detail::cacheline_size) Item* q;                       ///< Pointer to the array of queue slots (items).
    alignas(detail::cacheline_size) std::atomic<uint64_t> iwrite_; ///< Global write index for the queue.
    alignas(detail::cacheline_size) std::atomic<uint64_t> iread_;  ///< Global read index for the queue.
};

/**
 * @struct spsc_queue
 * @brief Lock-free single-producer single-consumer queue.
 *
 * @tparam T Element type.
 */
template<class T>
struct spsc_queue {
    spsc_queue(uint64_t cap) : capacity(align2n(cap)), mask(capacity - 1)
    {
        cached_write = capacity - 1;
        uint64_t us = 1;
        q = nullptr;
        for (;;) {
            if (std::is_pod_v<Item>) {
                q = static_cast<Item*>(malloc(sizeof(Item) * capacity));
            } else {
                q = new (std::nothrow) Item[capacity];
            }
            if (q) break;
            std::this_thread::sleep_for(std::chrono::microseconds(us));
            us = us << 1;
        }
        for (size_t i = 0; i < capacity; ++i) {
            q[i].valid.store(false, std::memory_order_relaxed);
        }

        iwrite_ = 0;
        iread_.store(0, std::memory_order_relaxed);
    }

    ~spsc_queue()
    {
        if (std::is_pod_v<Item>) {
            free(q);
        } else {
            delete[] q;
        }
    }

    template <class Data>
    inline bool try_push(Data&& data)
    {
        if (cached_write == 0) {
            uint64_t iread = iread_.load(std::memory_order_acquire);
            cached_write = iread + capacity - 1 - iwrite_;
            if (cached_write == 0) {
                return false;
            }
        }

        auto& i = q[iwrite_ & mask];
        i.data = std::forward<Data>(data);
        i.valid.store(true, std::memory_order_release);
        iwrite_++;
        cached_write--;
        return true;
    }

    template <class R>
    inline bool try_pop(R& result)
    {
        auto& i = q[iread_.load(std::memory_order_relaxed) & mask];
        if (!i.valid.load(std::memory_order_acquire)) {
            return false;
        }

        if constexpr(std::is_same_v<R, T>) {
            result = std::move(i.data);
        } else {
            result(i.data);
        }
        i.valid.store(false, std::memory_order_relaxed);
        iread_.fetch_add(1, std::memory_order_release);
        return true;
    }

private:
    uint64_t capacity;
    uint64_t mask;
    uint64_t cached_write;
    struct Item {
        T data;
        std::atomic<bool> valid;
    };
    Item* q;

    alignas(detail::cacheline_size) uint64_t iwrite_ = 0;
    alignas(detail::cacheline_size) std::atomic<uint64_t> iread_ = 0;
};

template <bool MultiProducer, bool multi_consumer, typename T>
struct lf_queue: mpmc_queue<T> {
    using mpmc_queue<T>::mpmc_queue;
};
template <typename T>
struct lf_queue<false, false, T>: spsc_queue<T> {
    using spsc_queue<T>::spsc_queue;
};

/**
 * @brief Structure representing a pointer-based task.
 */
struct ptr_task {
    void(*f)(void*); ///< Function pointer.
    void* arg;  ///< Argument pointer.
};

/**
 * @struct runnable_queue
 * @brief Runnable queue based on a given queue type.
 *
 * @tparam Queue Queue template type.
 */
template<template<class> class Queue>
struct runnable_queue : Queue<ptr_task> {
    /**
     * @brief Constructs a runnable queue.
     *
     * @param depth Queue depth.
     * @param name Queue name.
     */
    runnable_queue(uint64_t depth, const std::string& name) : Queue<ptr_task>(depth), name(name) {}

    /**
     * @brief Attempts to run a task from the queue.
     *
     * @return True if a task was run, false otherwise.
     */
    inline bool try_run()
    {
        ptr_task job;
        auto suc = this->try_pop(job);
        if (suc) {
            FFRT_API_TRACE_INT64(name.c_str(), this->size());
            job.f(job.arg);
            return true;
        }
        return false;
    }

    /**
     * @brief Pushes a task into the queue with a given policy.
     *
     * @tparam policy Push policy (0: sleep, 1: run).
     * @param f Function pointer.
     * @param p Argument pointer.
     */
    template <int Policy>
    inline void push(void(*f)(void*), void* p)
    {
        uint64_t us = 1;
        while (!this->try_push(ptr_task{f, p})) {
            if constexpr(Policy == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(us));
                us = us << 1;
            } else if constexpr(Policy == 1) {
                try_run();
            }
        }
        FFRT_API_TRACE_INT64(name.c_str(), this->size());
    }

    const std::string name; ///< Queue name.
};

/**
 * @struct clock
 * @brief High-resolution clock utilities.
 */
struct clock {
    using stamp = std::chrono::time_point<std::chrono::high_resolution_clock>;

    /**
     * @brief Returns the current time stamp.
     */
    static inline stamp now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    /**
     * @brief Returns the nanoseconds between two time stamps.
     *
     * @param from Start time.
     * @param to End time (default: now).
     * @return Nanoseconds between from and to.
     */
    static inline uint64_t ns(const stamp& from, stamp to = now())
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(to - from).count());
    }
};

/**
 * @struct fiber
 * @brief Lightweight fiber implementation.
 *
 * @tparam UsageId Usage identifier.
 * @tparam FiberLocal Type for fiber-local storage.
 * @tparam ThreadLocal Type for thread-local storage.
 */
#ifdef _ffrt_has_fiber_feature
template <int UsageId = 0, class FiberLocal = char, class ThreadLocal = char>
struct fiber : detail::non_copyable {
    /**
     * @brief Thread environment for fiber execution.
     */
    struct thread_env :  detail::non_copyable {
        fiber* cur = nullptr;
        bool (*cond)(void*) = nullptr;
        ThreadLocal tl;
    };

    /**
     * @brief Returns the thread-local environment.
     */
    static __attribute__((noinline)) thread_env& env()
    {
        static thread_local thread_env ctx;
        return ctx;
    }

    /**
     * @brief Initializes a fiber with a function and stack.
     *
     * @param f Function to run.
     * @param stack Stack memory.
     * @param stack_size Stack size.
     * @return Pointer to the created fiber.
     */
    static fiber* init(std::function<void()>&& f, void* stack, size_t stack_size)
    {
        auto c = static_cast<fiber*>(stack);
        if (ffrt_fiber_init(&c->fb, reinterpret_cast<void(*)(void*)>(fiber_entry), c,
            static_cast<char*>(stack) + sizeof(fiber), stack_size - sizeof(fiber))) {
            return nullptr;
        }
        new(&c->fn) std::function<void()>(std::forward<std::function<void()>>(f));
        new(&c->local_) FiberLocal;
        c->id_ = idx.fetch_add(1, std::memory_order_relaxed);
        FFRT_API_LOGD("fiber %lu create", c->id_);
        return c;
    }

    /**
     * @brief Destroys the fiber.
     */
    inline void destroy()
    {
        FFRT_API_LOGD("fiber %lu destroy", id_);
        fn.~function<void()>();
        local_.~FiberLocal();
    }

    /**
     * @brief Starts the fiber execution.
     *
     * @return True if finished, false otherwise.
     */
    bool start()
    {
        bool done;
        auto& e = fiber::env();

        do {
            e.cond = nullptr;
            e.cur = this;
            FFRT_API_LOGD("job %lu switch in", id_);
            ffrt_fiber_switch(&link, &fb);
            FFRT_API_LOGD("job %lu switch out", id_);
            done = this->id_ == 0;
        } while (e.cond && !(e.cond)(this));
        e.cond = nullptr;
        return done;
    }

    /**
     * @brief Suspends the current fiber.
     *
     * @tparam is_final Whether this is the final suspension.
     * @param e Thread environment.
     * @param cond Condition function.
     */
    template<bool is_final = false>
    static inline void suspend(thread_env& e, bool (*cond)(void*) = nullptr)
    {
        auto j = e.cur;
        if constexpr(is_final) {
            j->id_ = 0;
        } else {
            e.cond = cond;
        }
        e.cur = nullptr;

        ffrt_fiber_switch(&j->fb, &j->link);
    }

    /**
     * @brief Suspends the current fiber.
     *
     * @tparam is_final Whether this is the final suspension.
     * @param cond Condition function.
     */
    template<bool is_final = false>
    static inline void suspend(bool (*cond)(void*) = nullptr)
    {
        suspend<is_final>(fiber::env(), cond);
    }

    /**
     * @brief Returns the fiber-local storage.
     */
    FiberLocal& local()
    {
        return local_;
    }

    uint64_t id()
    {
        return id_;
    }

private:

    /**
     * @brief Fiber entry function. Executes the user function and suspends the fiber upon completion.
     *
     * @param c Pointer to the fiber object.
     */
    static void fiber_entry(fiber* c)
    {
        c->fn();
        c->fn = nullptr;
        suspend<true>();
    }

    ffrt_fiber_t fb;          ///< Fiber context for execution.
    ffrt_fiber_t link;        ///< Link to the previous fiber context.
    std::function<void()> fn; ///< Function to be executed by the fiber.
    uint64_t id_;             ///< Fiber identifier.
    FiberLocal local_;        ///< Fiber-local storage.
    static inline std::atomic_uint64_t idx{1}; ///< Atomic counter for generating unique fiber IDs.
};
#endif

template <typename T>
struct type_size {
    static constexpr size_t value = sizeof(T);
};
template <>
struct type_size<void> {
    static constexpr size_t value = 0;
};

template<class R>
struct job_promise_obj : ref_obj<job_promise_obj<R>> {
    ~job_promise_obj()
    {
        if (is_ready()) {
            if constexpr(!std::is_void_v<R>) {
                ((R*)blob)->~R();
            }
        }
    }

    inline bool is_ready() const
    {
        return stat.load(std::memory_order_acquire) == ready;
    }

    template <bool TryRun = true, uint64_t BusyWaitUS = 100>
    inline decltype(auto) get(const std::function<bool()>& on_wait = nullptr)
    {
        if (!is_ready()) {
            wait<TryRun, BusyWaitUS>(on_wait);
        }
        if constexpr(std::is_void_v<R>) {
            return;
        } else {
            return (*(R*)blob);
        }
    }

    inline bool try_execute()
    {
        if (try_acquire()) {
            call();
            release();
            return true;
        }
        return false;
    }

    void* owner;
private:
    job_promise_obj(std::function<R()>&& func) : func(std::forward<std::function<R()>>(func)) {}
    inline bool try_acquire()
    {
        int32_t exp = pending;
        return stat.compare_exchange_strong(exp, executing, std::memory_order_acquire, std::memory_order_relaxed);
    }

    inline void release()
    {
        if (stat.load() != executing) abort();
        stat.store(ready, std::memory_order_release);
        sev();
        if (waiter.exchange(0) == 1) {
            waiter.notify_all();
        }
    }

    inline void call()
    {
        if constexpr (std::is_void_v<R>) {
            func();
        } else {
            new (blob) R(func());
        }
    }

    template <bool TryRun = true, uint64_t BusyWaitUS = 100>
    inline void wait(const std::function<bool()>& on_wait = nullptr)
    {
        if (TryRun && try_acquire()) {
            call();
            release();
            return;
        }

        if (on_wait) {
            while (!is_ready() && on_wait());
            if (is_ready()) return;
        }
        if (BusyWaitUS) {
            auto s = clock::now();
            do {
                if (is_ready()) return;
                wfe();
            } while (clock::ns(s) < BusyWaitUS * 1000);
        }

        while (!is_ready()) {
            waiter = 1;
            if (!is_ready()) {
                waiter.wait(1);
            }
            if (waiter.exchange(0) == 1) {
                waiter.notify_all();
            }
        }
    }

    enum {
        pending,
        executing,
        ready
    };

    std::function<R()> func;
    char blob[type_size<R>::value];
    std::atomic_int32_t stat{pending};
    atomic_wait waiter = 0;

    friend ref_obj<job_promise_obj<R>>;
};

/**
 * @struct job_promise
 * @brief Provide the function of executing the task and getting the result of the task.
 *
 * @tparam R Element type.
 */
template<class R>
struct job_promise : public job_promise_obj<R>::ptr {
    using Base = typename job_promise_obj<R>::ptr;
    using Base::Base;
    job_promise(const Base& p) : Base(p) {}

    /**
    * @brief This function is used to determine whether a task has been completed or not.
    *
    * @return Returning `true` indicates that the task has been completed, while `false`
    *         indicates that the task has not been executed completely.
    * @since 20
    */
    inline bool is_ready() const
    {
        return Base::get()->is_ready();
    }

    /**
    * @brief Obtain the execution results of dependent tasks.
    *
    * @tparam TryRun The waiting thread directly executes the `job_promise` dependent task or not.
    * @tparam BusyWaitUS The current waiting thread will enter sleep after waiting for `BusyWaitUS`,
    *         and will not be woken up until the task is completed.
    * @param on_wait The function that needs to be executed by the current thread during the waiting
    *        process for task execution.
    * @return Task execution result.
    * @since 20
    */
    template <bool TryRun = true, uint64_t BusyWaitUS = 100>
    inline decltype(auto) get(const std::function<bool()>& on_wait = nullptr)
    {
        return Base::get()->template get<TryRun, BusyWaitUS>(on_wait);
    }

    /**
    * @brief If the job hasn't been executed yet, execute it directly in the current thread;
    *        otherwise, return directly.
    *
    * @return Return false if the job has executed or is executing, otherwise execute it and return true;
    * @since 20
    */
    inline bool try_execute()
    {
        return Base::get()->try_execute();
    }

    template <uint64_t UsageId>
    friend struct job_partner;
};

template<class R>
static inline job_promise<R> make_job_promise(std::function<R()>&& func)
{
    return job_promise_obj<R>::make(std::forward<std::function<R()>>(func));
}
}
#endif