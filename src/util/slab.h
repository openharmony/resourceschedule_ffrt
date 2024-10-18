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

#ifndef UTIL_SLAB_HPP
#define UTIL_SLAB_HPP

#include <new>
#include <vector>
#include <mutex>
#ifdef FFRT_BBOX_ENABLE
#include <unordered_set>
#endif
#include <sys/mman.h>
#include "dfx/log/ffrt_log_api.h"
#include "spmc_queue.h"
#include "sync/sync.h"

namespace ffrt {
const std::size_t BatchAllocSize = 32 * 1024;
#ifdef FFRT_BBOX_ENABLE
constexpr uint32_t ALLOCATOR_DESTRUCT_TIMESOUT = 1000;
#endif

#ifndef FFRT_ALLOCATOR_MMAP_SIZE
#define FFRT_ALLOCATOR_MMAP_SIZE (8 * 1024 * 1024)
#endif

template <typename T, size_t MmapSz = BatchAllocSize>
class SimpleAllocator {
public:
    SimpleAllocator(const SimpleAllocator&) = delete;
    SimpleAllocator(SimpleAllocator&&) = delete;
    SimpleAllocator& operator=(const SimpleAllocator&) = delete;
    SimpleAllocator& operator=(SimpleAllocator&&) = delete;
    fast_mutex lock;

    static SimpleAllocator<T>* Instance(std::size_t size = sizeof(T))
    {
        static SimpleAllocator<T> ins(size);
        return &ins;
    }

    // NOTE: call constructor after AllocMem
    static T* AllocMem()
    {
        return Instance()->Alloc();
    }

    // NOTE: call destructor before FreeMem
    static void FreeMem(T* t)
    {
        t->~T();
        // unlock()内部lck记录锁的状态为非持有状态，析构时访问状态变量为非持有状态，则不访问实际持有的mutex
        // return之前的lck析构不产生UAF问题，因为return之前随着root析构，锁的内存被释放
        Instance()->free(t);
    }

    // only used for BBOX
    static std::vector<void *> getUnfreedMem()
    {
        return Instance()->getUnfreed();
    }

    static void LockMem()
    {
        return Instance()->SimpleAllocatorLock();
    }

    static void UnlockMem()
    {
        return Instance()->SimpleAllocatorUnLock();
    }
private:
    SpmcQueue primaryCache;
#ifdef FFRT_BBOX_ENABLE
    std::unordered_set<T*> secondaryCache;
#endif
    std::size_t TSize;
    T* basePtr = nullptr;
    std::size_t count = 0;

    std::vector<void *> getUnfreed()
    {
        std::vector<void *> ret;
#ifdef FFRT_BBOX_ENABLE
        ret.reserve(MmapSz / TSize + secondaryCache.size());
        char* p = reinterpret_cast<char*>(basePtr);
        for (std::size_t i = 0; i + TSize <= MmapSz; i += TSize) {
            if (basePtr != nullptr &&
                primaryCache.FindElement(reinterpret_cast<void *>(p + i)) == false) {
                ret.push_back(reinterpret_cast<void *>(p + i));
            }
        }
        for (auto ite = secondaryCache.cbegin(); ite != secondaryCache.cend(); ite++) {
            ret.push_back(reinterpret_cast<void *>(*ite));
        }
#endif
        return ret;
    }

    void SimpleAllocatorLock()
    {
        lock.lock();
    }

    void SimpleAllocatorUnLock()
    {
        lock.unlock();
    }

    void init()
    {
        lock.lock();
        if (basePtr) {
            lock.unlock();
            return;
        }

        char* p = reinterpret_cast<char*>(operator new(MmapSz));
        count = MmapSz / TSize;
        primaryCache.Init(count);
        for (std::size_t i = 0; i + TSize <= MmapSz; i += TSize) {
            primaryCache.PushTail(p + i);
        }
        basePtr = reinterpret_cast<T*>(p);
        lock.unlock();
    }

    T* Alloc()
    {
        // 未初始化
        if (primaryCache.GetCapacity() == 0) {
            init();
        }
        // 一级cache已耗尽
        if (primaryCache.GetLength()) {
            T* t = reinterpret_cast<T*>(::operator new(TSize));
#ifdef FFRT_BBOX_ENABLE
            lock.lock();
            secondaryCache.insert(t);
            lock.unlock();
#endif
            return t;
        }
        return static_cast<T*>(primaryCache.PopHead());
    }

    void free(T* t)
    {
        if (basePtr && basePtr <= t &&
            static_cast<size_t>(reinterpret_cast<uintptr_t>(t)) <
            static_cast<size_t>(reinterpret_cast<uintptr_t>(basePtr)) + MmapSz) {
            primaryCache.PushTail(t);
            return;
        }

#ifdef FFRT_BBOX_ENABLE
        lock.lock();
        secondaryCache.erase(t);
        lock.unlock();
#endif
        ::operator delete(t);
    }

    SimpleAllocator(std::size_t size = sizeof(T)) : TSize(size)
    {
    }
    ~SimpleAllocator()
    {
        std::unique_lock<decltype(lock)> lck(lock);
        if (basePtr == nullptr) {
            return;
        }
#ifdef FFRT_BBOX_ENABLE
        uint32_t try_cnt = ALLOCATOR_DESTRUCT_TIMESOUT;
        std::size_t reserved = MmapSz / TSize;
        while (try_cnt > 0) {
            if (primaryCache.GetLength() == reserved && secondaryCache.size() == 0) {
                break;
            }
            lck.unlock();
            usleep(1000);
            try_cnt--;
            lck.lock();
        }
        if (try_cnt == 0) {
            FFRT_LOGE("clear allocator failed");
        }
        for (auto ite = secondaryCache.cbegin(); ite != secondaryCache.cend(); ite++) {
            ::operator delete(*ite);
        }
#endif
        ::operator delete(basePtr);
        FFRT_LOGI("destruct SimpleAllocator");
    }
};

template <typename T, std::size_t MmapSz = FFRT_ALLOCATOR_MMAP_SIZE>
class QSimpleAllocator {
    std::size_t TSize;
    std::size_t curAllocated;
    std::size_t maxAllocated;
    std::mutex lock;
    std::vector<T*> cache;
    uint32_t flags = MAP_ANONYMOUS | MAP_PRIVATE;

    bool expand()
    {
        const int prot = PROT_READ | PROT_WRITE;
        char* p = reinterpret_cast<char*>(mmap(nullptr, MmapSz, prot, flags, -1, 0));
        if (p == (char*)MAP_FAILED) {
            if ((flags & MAP_HUGETLB) != 0) {
                flags = MAP_ANONYMOUS | MAP_PRIVATE;
                p = reinterpret_cast<char*>(mmap(nullptr, MmapSz, prot, flags, -1, 0));
            }
            if (p == (char*)MAP_FAILED) {
                perror("mmap");
                return false;
            }
        }
        for (std::size_t i = 0; i + TSize <= MmapSz; i += TSize) {
            cache.push_back(reinterpret_cast<T*>(p + i));
        }
        return true;
    }

    T* Alloc()
    {
        T* p = nullptr;
        lock.lock();
        if (cache.empty()) {
            if (!expand()) {
                lock.unlock();
                return nullptr;
            }
        }
        p = cache.back();
        ++curAllocated;
        maxAllocated = std::max(curAllocated, maxAllocated);
        cache.pop_back();
        lock.unlock();
        return p;
    }

    void free(T* p)
    {
        lock.lock();
        --curAllocated;
        cache.push_back(p);
        lock.unlock();
    }

    void release()
    {
        T* p = nullptr;
        lock.lock();
        FFRT_LOGD("coroutine release with waterline %d, cur occupied %d, cached size %d",
            maxAllocated, curAllocated, cache.size());
        size_t reservedCnt = maxAllocated - curAllocated + 1; // reserve additional one for robustness
        maxAllocated = curAllocated;
        while (cache.size() > reservedCnt) {
            p = cache.back();
            cache.pop_back();
            int ret = munmap(p, TSize);
            if (ret != 0) {
                FFRT_LOGE("munmap failed with errno: %d", errno);
            }
        }
        lock.unlock();
    }

    QSimpleAllocator()
    {
    }

public:
    explicit QSimpleAllocator(std::size_t size = sizeof(T)) : curAllocated(0), maxAllocated(0)
    {
        std::size_t p_size = static_cast<std::size_t>(getpagesize());
        // manually align the size to the page size
        TSize = (size - 1 + p_size) & -p_size;
        if (MmapSz % TSize != 0) {
            FFRT_LOGE("MmapSz is not divisible by TSize which may cause memory leak!");
        }
    }
    QSimpleAllocator(QSimpleAllocator const&) = delete;
    void operator=(QSimpleAllocator const&) = delete;

    static QSimpleAllocator<T, MmapSz>* Instance(std::size_t size)
    {
        static QSimpleAllocator<T, MmapSz> ins(size);
        return &ins;
    }

    static T* AllocMem(std::size_t size = sizeof(T))
    {
        return Instance(size)->Alloc();
    }

    static void FreeMem(T* p, std::size_t size = sizeof(T))
    {
        Instance(size)->free(p);
    }

    static void releaseMem(std::size_t size = sizeof(T))
    {
        Instance(size)->release();
    }
};
} // namespace ffrt
#endif /* UTIL_SLAB_H */
