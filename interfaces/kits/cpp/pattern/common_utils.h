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

#ifndef FFRT_COMMON_UTILS_H
#define FFRT_COMMON_UTILS_H

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
#include "c/fiber.h"

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
namespace detail {
    static constexpr uint64_t cacheline_size = 64;

    struct non_copyable {
    protected:
        non_copyable() = default;
        ~non_copyable() = default;
        non_copyable(const non_copyable&) = delete;
        non_copyable& operator=(const non_copyable&) = delete;
    };
}

static inline void wfe()
{
#if (defined __aarch64__ || defined __arm__)
    __asm__ volatile("wfe" : : : "memory");
#endif
}

static inline constexpr uint64_t align2n(uint64_t x)
{
    uint64_t i = 1;
    uint64_t t = x;
    while (x >>= 1) {
        i <<= 1;
    }
    return (i < t) ? (i << 1) : i;
}

struct futex {
    static inline void wait(int* uaddr, int val)
    {
        FFRT_API_LOGD("futex wait in %p", uaddr);
        int r = call(uaddr, FUTEX_WAIT_PRIVATE, val, nullptr, 0);
        FFRT_API_LOGD("futex wait %p ret %d", uaddr, r);
    }

    static inline void wake(int* uaddr, int num)
    {
        int r = call(uaddr, FUTEX_WAKE_PRIVATE, num, nullptr, 0);
        FFRT_API_LOGD("futex wake %p ret %d", uaddr, r);
    }

private:
    static inline int call(int* uaddr, int op, int val, const struct timespec* timeout, int bitset)
    {
        return syscall(SYS_futex, uaddr, op, val, timeout, NULL, bitset);
    }
};

struct atomic_wait : std::atomic<int> {
    using std::atomic<int>::atomic;
    using std::atomic<int>::operator=;
    inline void wait(int val)
    {
        futex::wait((int*)this, val);
    }
    inline auto notify_one()
    {
        futex::wake((int*)this, 1);
    }
    inline void notify_all()
    {
        futex::wake((int*)this, INT_MAX);
    }
};

template <class T>
struct ref_obj {
    struct ptr {
        ptr() {}
        ptr(void* p) : p((T*)p) {}
        ~ptr()
        {
            reset();
        }

        inline ptr(ptr const& h)
        {
            *this = h;
        }
        inline ptr& operator=(ptr const& h)
        {
            if (this != &h) {
                p = h.p;
                if (p) {
                    p->inc_ref();
                }
            }
            return *this;
        }

        inline ptr(ptr&& h)
        {
            *this = std::move(h);
        }
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
        constexpr inline T* get()
        {
            return p;
        }
        constexpr inline const T* get() const
        {
            return p;
        }
        constexpr inline T* operator -> ()
        {
            return p;
        }
        constexpr inline const T* operator -> () const
        {
            return p;
        }
        inline operator void* () const
        {
            return p;
        }
        inline void reset()
        {
            if (p) {
                p->dec_ref();
                p = nullptr;
            }
        }
    private:
        T* p = nullptr;
    };

    template<class... Args>
    static inline ptr make(Args&& ... args)
    {
        auto p = new T(std::forward<Args>(args)...);
        FFRT_API_LOGD("%s new %p", __PRETTY_FUNCTION__, p);
        return ptr(p);
    }

    template<class... Args>
    static ptr& singleton(Args&& ... args)
    {
        static ptr s = make(std::forward<Args>(args)...);
        return s;
    }

    inline void inc_ref()
    {
        ref.fetch_add(1, std::memory_order_relaxed);
    }

    inline void dec_ref()
    {
        if (ref.fetch_sub(1, std::memory_order_relaxed) == 1) {
            FFRT_API_LOGD("%s delete %p", __PRETTY_FUNCTION__, this);
            delete (T*)this;
        }
    }
private:
    std::atomic_uint64_t ref{1};
};

template <typename T>
struct mpmc_queue : detail::non_copyable {
    mpmc_queue(uint64_t cap) : capacity(align2n(cap)), mask(capacity - 1)
    {
        if (std::is_pod_v<Item>) {
            q = (Item*)malloc(sizeof(Item) * capacity);
        } else {
            q = new Item [capacity];
        }
        for (size_t i = 0; i < capacity; ++i) {
            q[i].iwrite_exp.store(i, std::memory_order_relaxed);
            q[i].iread_exp.store(-1, std::memory_order_relaxed);
        }

        iwrite_.store(0, std::memory_order_relaxed);
        iread_.store(0, std::memory_order_relaxed);
    }

    ~mpmc_queue()
    {
        if (std::is_pod_v<Item>) {
            free(q);
        } else {
            delete [] q;
        }
    }
    inline uint64_t size() const
    {
        auto head = iread_.load(std::memory_order_relaxed);
        return iwrite_.load(std::memory_order_relaxed) - head;
    }

    bool try_push(const T& data)
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
        i->data = data;
        i->iread_exp.store(iwrite, std::memory_order_release);
        return true;
    }

    bool try_pop(T& result)
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
        result = i->data;
        i->iwrite_exp.store(iread + capacity, std::memory_order_release);
        return true;
    }
private:
    uint64_t capacity;
    uint64_t mask;
    struct Item {
        T data;
        std::atomic<uint64_t> iwrite_exp; // expect write index after read
        std::atomic<uint64_t> iread_exp; // expect read index after write
    };

    alignas(detail::cacheline_size) Item* q;
    alignas(detail::cacheline_size) std::atomic<uint64_t> iwrite_; // global write index
    alignas(detail::cacheline_size) std::atomic<uint64_t> iread_; // global read index
};

using func_ptr = void(*)(void*);
struct ptr_task {
    func_ptr f;
    void* arg;
};

template<template<class> class Queue>
struct runnable_queue : Queue<ptr_task> {
    runnable_queue(uint64_t depth, const std::string& name) : Queue<ptr_task>(depth), name(name) {}

    inline bool try_run()
    {
        ptr_task job;
        auto suc = this->try_pop(job);
        if (!suc) {
            return false;
        }

        FFRT_API_TRACE_INT64(name.c_str(), this->size());
        job.f(job.arg);
        return true;
    }

    template <int policy>
    inline void push(func_ptr f, void* p)
    {
        uint64_t us = 1;
        while (!this->try_push({f, p})) {
            if constexpr(policy == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(us));
                us = us << 1;
            } else if constexpr(policy == 1) {
                try_run();
            }
        }
        FFRT_API_TRACE_INT64(name.c_str(), this->size());
    }

    const std::string name;
};

struct clock {
    using stamp = std::chrono::time_point<std::chrono::high_resolution_clock>;

    static inline stamp now()
    {
        return std::chrono::high_resolution_clock::now();
    }

    static inline uint64_t ns(const stamp& from, stamp to = now())
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(to - from).count());
    }
};

template <int UsageId = 0, class FiberLocal = char, class ThreadLocal = char>
struct fiber : detail::non_copyable {
    struct thread_env :  detail::non_copyable {
        fiber* cur = nullptr;
        bool (*cond)(void*) = nullptr;
        ThreadLocal tl;
    };

    static __attribute__((noinline)) thread_env& env()
    {
        static thread_local thread_env ctx;
        return ctx;
    }

    static fiber* init(std::function<void()>&& f, void* stack, size_t stack_size)
    {
        if (stack == nullptr || stack_size < sizeof(fiber) + min_stack_size) {
            return nullptr;
        }
        auto c = new (stack) fiber(std::forward<std::function<void()>>(f), stack_size);
        if (ffrt_fiber_init(&c->fb, (void(*)(void*))fiber_entry, c, (char*)stack + sizeof(fiber),
            stack_size - sizeof(fiber))) {
            c->~fiber<UsageId, FiberLocal, ThreadLocal>();
            return nullptr;
        }

        FFRT_API_LOGD("job %lu create", c->id);
        return c;
    }

    inline void destroy()
    {
        FFRT_API_LOGD("job %lu destroy", id);
        this->~fiber<UsageId, FiberLocal, ThreadLocal>();
    }

    bool start()
    {
        bool done;
        auto& e = fiber::env();

        do {
            e.cond = nullptr;
            e.cur = this;
            FFRT_API_LOGD("job %lu switch in", id);
            ffrt_fiber_switch(&link, &fb);
            FFRT_API_LOGD("job %lu switch out", id);
            done = this->done;
        } while (e.cond && !(e.cond)(this));
        e.cond = nullptr;
        return done;
    }

    template<bool is_final = false>
    static inline void suspend(thread_env& e, bool (*cond)(void*) = nullptr)
    {
        auto j = e.cur;
        if constexpr(is_final) {
            j->done = true;
        } else {
            e.cond = cond;
        }
        e.cur = nullptr;

        ffrt_fiber_switch(&j->fb, &j->link);
    }

    template<bool is_final = false>
    static inline void suspend(bool (*cond)(void*) = nullptr)
    {
        suspend<is_final>(fiber::env(), cond);
    }

    FiberLocal& local()
    {
        return local_;
    }

    uint64_t id;
private:
    static constexpr uint64_t min_stack_size = 32;

    fiber(std::function<void()>&& f, size_t stack_size)
    {
        fn = std::forward<std::function<void()>>(f);
        id = idx.fetch_add(1, std::memory_order_relaxed);
    }

    static void fiber_entry(fiber* c)
    {
        c->fn();
        c->fn = nullptr;
        suspend<true>();
    }

    ffrt_fiber_t fb;
    ffrt_fiber_t link;
    std::function<void()> fn;
    bool done = false;
    FiberLocal local_;
    static inline std::atomic_uint64_t idx{0};
};
}
#endif