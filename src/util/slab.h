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
#ifndef _MSC_VER
#include <sys/mman.h>
#endif
#include "sync/sync.h"

namespace ffrt {

const std::size_t BatchAllocSize = 2 * 1024 * 1024;

template <typename T, size_t MmapSz = BatchAllocSize>
class SimpleAllocator {
public:
    SimpleAllocator(SimpleAllocator const&) = delete;
    void operator=(SimpleAllocator const&) = delete;

    static SimpleAllocator<T>* instance()
    {
        static SimpleAllocator<T> ins;
        return &ins;
    }

    // NOTE: call constructor after allocMem
    static T* allocMem()
    {
        return instance()->alloc();
    }

    // NOTE: call destructor before freeMem
    static void freeMem(T* t)
    {
        t->~T();
        instance()->free(t);
    }

    // only used for BBOX
    static std::vector<T*> getUnfreedMem()
    {
        return instance()->getUnfreed();
    }
private:
#ifdef MUTEX_PERF // Mutex Lock&Unlock Cycles Statistic
    xx::mutex lock {"SimpleAllocator::lock"};
#else
    fast_mutex lock;
#endif
    std::vector<T*> cache;
    std::vector<T*> mempools;
    uint32_t count = 0;

    void expand()
    {
        T* bufs = reinterpret_cast<T*>(::operator new(MmapSz));
        mempools.push_back(bufs);
        cache.reserve(cache.size() + MmapSz / sizeof(T));
        for (std::size_t i = 0; i < MmapSz / sizeof(T); ++i) {
            cache.push_back(&bufs[i]);
        }
        count = MmapSz / sizeof(T);
    }

    std::vector<T*> getUnfreed()
    {
        lock.lock();
        std::vector<T*> ret;
        ret.reserve(mempools.size() * MmapSz / sizeof(T) - cache.size());
        for (auto bufs : mempools) {
            for (std::size_t i = 0; i < MmapSz / sizeof(T); ++i) {
                if (std::find(cache.begin(), cache.end(), &bufs[i]) == cache.end()) {
                    ret.push_back(&bufs[i]);
                }
            }
        }
        lock.unlock();
        return ret;
    }

    inline T* alloc()
    {
        lock.lock();
        if (count == 0) {
            expand();
        }
        T* t = cache.back();
        cache.pop_back();
        count--;
        lock.unlock();
        return t;
    }

    void free(T* t)
    {
        lock.lock();
        cache.push_back(t);
        count++;
        lock.unlock();
    }

    SimpleAllocator()
    {
    }
    ~SimpleAllocator()
    {
        for (auto ite = mempools.cbegin(); ite != mempools.cend(); ite++) {
            ::operator delete(*ite);
        }
    }
};

#ifndef _MSC_VER
template <typename T, std::size_t MmapSz = 16 * 1024 * 1024>
class QSimpleAllocator {
    static QSimpleAllocator<T, MmapSz>* instance(std::size_t size)
    {
        static QSimpleAllocator<T, MmapSz> ins(size);
        return &ins;
    }
    std::size_t TSize;
    std::mutex lock;
    std::vector<T*> cache;
    uint32_t flags = MAP_ANONYMOUS | MAP_PRIVATE;

    bool expand()
    {
        const int prot = PROT_READ | PROT_WRITE;
        std::size_t sz = (TSize + 15UL) & -16UL;
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
        for (std::size_t i = 0; i + sz <= MmapSz; i += sz) {
            cache.push_back(reinterpret_cast<T*>(p + i));
        }
        return true;
    }

    T* alloc()
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
        cache.pop_back();
        lock.unlock();
        return p;
    }

    void free(T* p)
    {
        lock.lock();
        cache.push_back(p);
        lock.unlock();
    }

    QSimpleAllocator()
    {
    }

public:
    QSimpleAllocator(std::size_t size = sizeof(T))
    {
        TSize = size;
    }
    QSimpleAllocator(QSimpleAllocator const&) = delete;
    void operator=(QSimpleAllocator const&) = delete;

    static T* allocMem(std::size_t size = sizeof(T))
    {
        return instance(size)->alloc();
    }

    static void freeMem(T* p, std::size_t size = sizeof(T))
    {
        instance(size)->free(p);
    }
};
#endif

} // namespace ffrt

#endif /* UTIL_SLAB_H */
