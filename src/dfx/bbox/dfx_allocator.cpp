/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "dfx_allocator.h"

#include <fcntl.h>
#include <securec.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <unistd.h>
#include <sys/cdefs.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/user.h>
#include "musl_malloc_dispatch_table.h"
#define HOOK_ENABLE
#include "musl_preinit_common.h"

static struct MallocDispatchType g_dfxCustomMallocDispatch;
static const size_t ALIGN_SIZE = 16;
static const size_t DFX_PAGE_SIZE = 4096;
static const size_t DFX_PAGE_MASK = ~(DFX_PAGE_SIZE - 1);
static const size_t DFX_MEMPOOL_MAX_BLOCK_SIZE = 1024;
static const uint32_t DFX_MEMPOOL_MIN_TYPE = 4;
static const uint32_t DFX_MEMPOOL_MAX_TYPE = 10;
static const uint32_t DFX_MMAP_TYPE = 111;
static const size_t PAGE_INFO_SIZE = ((sizeof(PageInfo) + ALIGN_SIZE - 1) & ~(ALIGN_SIZE - 1));
static const char DFX_MEM_PAGE_SIGN[DFX_MEMPOOL_TAG_SIZE] = {'D', 'F', 'X', 1};
static DfxAllocator g_dfxAllocator = {
    .initFlag = 0,
    .pageList = nullptr,
};

static inline uintptr_t PageStart(uintptr_t addr)
{
    return (addr & DFX_PAGE_MASK);
}

static inline uintptr_t PageEnd(uintptr_t addr)
{
    return PageStart(addr + (DFX_PAGE_SIZE - 1));
}

static inline size_t AlignRoundUp(size_t val, size_t align)
{
    size_t size = align;
    if (size == 0) {
        size = 1;
    }
    return ((val + size - 1) & ~(size - 1));
}

static void AddPage(PageInfo** pageList, PageInfo* page)
{
    if (pageList == nullptr || page == nullptr) {
        return;
    }
    page->next = *pageList;
    page->prev = nullptr;
    if (*pageList) {
        (*pageList)->prev = page;
    }
    *pageList = page;
}

static void RemovePage(PageInfo** pageList, PageInfo* page)
{
    if (pageList == nullptr || page == nullptr) {
        return;
    }
    if (page->prev) {
        page->prev->next = page->next;
    }
    if (page->next) {
        page->next->prev = page->prev;
    }
    if (*pageList == page) {
        *pageList = page->next;
    }
    page->prev = nullptr;
    page->next = nullptr;
}

static void MempoolAddPage(DfxMempool* mempool, PageInfo* page)
{
    if (mempool == nullptr) {
        printf("MempoolAddPage Invalid mempool!\n");
        return;
    }
    return AddPage(&(mempool->pageList), page);
}

static void MempoolRemovePage(DfxMempool* mempool, PageInfo* page)
{
    if (mempool == nullptr) {
        printf("MempoolRemovePage Invalid mempool!\n");
        return;
    }
    return RemovePage(&(mempool->pageList), page);
}

static void MempoolAllocPage(DfxMempool* mempool)
{
    void* mptr = mmap(nullptr, DFX_PAGE_SIZE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mptr == MAP_FAILED) {
        printf("Mempool AllocPage mmap failed!\n");
        return;
    }
    PageInfo* page = (PageInfo*)(mptr);
    // fill PageInfo
    if (memcpy_s(page->tag.tagInfo, sizeof(page->tag.tagInfo),
        DFX_MEM_PAGE_SIGN, sizeof(DFX_MEM_PAGE_SIGN)) != EOK) {
        munmap(mptr, DFX_PAGE_SIZE);
        printf("Mempool AllocPage fill tag failed!\n");
        return;
    }
    page->tag.type = mempool->type;
    page->tag.mempool = mempool;
    page->freeBlocksCnt = mempool->blocksPerPage;
    // Page firstblockaddr = page start addr + PageInfo size round up by blocksize
    uintptr_t firstBlockAddr = AlignRoundUp(reinterpret_cast<uintptr_t>(page + 1), mempool->blockSize);
    BlockInfo* firstBlock = reinterpret_cast<BlockInfo*>(firstBlockAddr);
    // here first block is not alloced block，freeblockList point it
    firstBlock->freeBlocksCnt = mempool->blocksPerPage;
    page->freeBlockList = firstBlock;
    MempoolAddPage(mempool, page);
    mempool->freePagesCnt++;
}

static void MempoolFreePage(DfxMempool* mempool, PageInfo* page)
{
    if (page->freeBlocksCnt != mempool->blocksPerPage) {
        printf("MempoolFreePage Invalid Page free cnt!\n");
        return;
    }
    MempoolRemovePage(mempool, page);
    munmap(page, DFX_PAGE_SIZE);
    mempool->freePagesCnt--;
}

static void* MempoolAlloc(DfxMempool* mempool)
{
    if (mempool == nullptr || mempool->blockSize == 0) {
        return nullptr;
    }
    // first alloc memory block, or all page memory blocks have been allocated
    if (mempool->pageList == nullptr) {
        MempoolAllocPage(mempool);
    }
    PageInfo* page = mempool->pageList;
    if (page == nullptr || page->freeBlockList == nullptr) {
        printf("MempoolAlloc Alloc Page Failed or Invalid blocklist!\n");
        return nullptr;
    }
    BlockInfo* block = page->freeBlockList;
    if (block->freeBlocksCnt > 1) {
        // freeBlocksCnt > 1 stand for page have free block's room
        // when a block will be allocated,
        // freeblocklist have no node, need alloc a new block in page room, then add to freeblocklist
        BlockInfo* nextfree = reinterpret_cast<BlockInfo*>(reinterpret_cast<uint8_t*>(block) + mempool->blockSize);
        nextfree->next = block->next;
        nextfree->freeBlocksCnt = block->freeBlocksCnt - 1;
        page->freeBlockList = nextfree;
    } else {
        // last freed block in freeblocklist or the last one block in page
        page->freeBlockList = block->next;
    }

    // new alloc page, free cnt -1 when used, free cnt +1 when allocated
    if (page->freeBlocksCnt == mempool->blocksPerPage) {
        mempool->freePagesCnt--;
    }
    if (page->freeBlocksCnt > 0) {
        page->freeBlocksCnt--;
    }
    (void)memset_s(block, mempool->blockSize, 0, mempool->blockSize);
    // when page's blocks all allocated, remove from pagelist but not free
    // then pagelist will be point page which have unused blocks
    // or pagelist be NULL will alloc new page
    if (page->freeBlocksCnt == 0) {
        MempoolRemovePage(mempool, page);
    }
    return reinterpret_cast<void*>(block);
}

static void MempoolFree(DfxMempool* mempool, void* ptr)
{
    PageInfo * const page = reinterpret_cast<PageInfo*>(PageStart(reinterpret_cast<uintptr_t>(ptr)));

    if (mempool == nullptr || ptr == nullptr ||
        (reinterpret_cast<uintptr_t>(ptr)) % (mempool->blockSize) != 0) {
        printf("MempoolFree Invalid mempool or address!\n");
        return;
    }
    // find ptr's page, and page's freeblocklist
    (void)memset_s(ptr, mempool->blockSize, 0, mempool->blockSize);
    BlockInfo* block = (BlockInfo*)(ptr);
    block->next = page->freeBlockList;
    block->freeBlocksCnt = 1;
    page->freeBlockList = block;
    page->freeBlocksCnt++;

    // all page's blocks have been freed, unmap the page
    // page have only one block be freed, need add page to pageList
    if (page->freeBlocksCnt == mempool->blocksPerPage) {
        mempool->freePagesCnt++;
        if (mempool->freePagesCnt >= 1) {
            MempoolFreePage(mempool, page);
        }
    } else if (page->freeBlocksCnt == 1) {
        MempoolAddPage(mempool, page);
    }
}

static inline uint32_t SelectMempoolType(size_t num)
{
    uint32_t res = 0;
    size_t n = num - 1;
    // alloc size (1~1024), use diffrent block size mempool
    // The 16-byte size range is the smallest.
    // Each subsequent level is twice that of the previous level. The maximum is 1024 bytes.
    while (n != 0) {
        res++;
        n >>= 1;
    }
    return res;
}

static void InitDfxAllocator(void)
{
    for (uint32_t i = 0; i < DFX_MEMPOOLS_NUM; i++) {
        g_dfxAllocator.dfxMempoolBuf[i].type = i + DFX_MEMPOOL_MIN_TYPE;
        g_dfxAllocator.dfxMempoolBuf[i].blockSize = (1UL << g_dfxAllocator.dfxMempoolBuf[i].type);
        g_dfxAllocator.dfxMempoolBuf[i].blocksPerPage =
            (DFX_PAGE_SIZE - sizeof(PageInfo)) / (g_dfxAllocator.dfxMempoolBuf[i].blockSize);
        g_dfxAllocator.dfxMempoolBuf[i].freePagesCnt = 0;
        g_dfxAllocator.dfxMempoolBuf[i].pageList = nullptr;
    }
    g_dfxAllocator.initFlag = 1;
}

static inline PageInfo* GetPageUnchecked(void* ptr)
{
    uintptr_t pageHead = PageStart(reinterpret_cast<uintptr_t>(ptr) - PAGE_INFO_SIZE);
    return reinterpret_cast<PageInfo*>(pageHead);
}

static inline PageInfo* GetPage(void* ptr)
{
    PageInfo* page = GetPageUnchecked(ptr);
    if (memcmp(page->tag.tagInfo, DFX_MEM_PAGE_SIGN, sizeof(DFX_MEM_PAGE_SIGN)) != 0) {
        printf("GetPage untagged address!\n");
        return nullptr;
    }
    return page;
}

static void* AllocMmap(size_t align, size_t size)
{
    const size_t headSize = AlignRoundUp(PAGE_INFO_SIZE, align);
    size_t allocSize = 0;

    // mmap size page allign
    if (__builtin_add_overflow(headSize, size, &allocSize) ||
        PageEnd(allocSize) < allocSize) {
        printf("Invalid mmap size!\n");
        return nullptr;
    }
    allocSize = PageEnd(allocSize);
    void* mptr = mmap(nullptr, allocSize, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mptr == MAP_FAILED) {
        printf("AllocMmap failed!\n");
        return nullptr;
    }
    void* result = reinterpret_cast<void*>(reinterpret_cast<char*>(mptr) + headSize);
    PageInfo* page = GetPageUnchecked(result);
    if (memcpy_s(page->tag.tagInfo, sizeof(page->tag.tagInfo),
        DFX_MEM_PAGE_SIGN, sizeof(DFX_MEM_PAGE_SIGN)) != EOK) {
        munmap(mptr, allocSize);
        printf("AllocMmap fill tag failed!\n");
        return nullptr;
    }
    page->tag.type = DFX_MMAP_TYPE;
    page->tag.mMapAllocSize = allocSize;
    page->prev = nullptr;
    page->next = nullptr;
    page->freeBlockList = nullptr;
    page->freeBlocksCnt = 0;
    AddPage(&(g_dfxAllocator.pageList), page);
    return result;
}

static void* AllocImpl(size_t align, size_t size)
{
    uint32_t type = 0;
    DfxMempool* mempool = nullptr;

    if (g_dfxAllocator.initFlag == 0) {
        InitDfxAllocator();
    }
    if (size > DFX_MEMPOOL_MAX_BLOCK_SIZE) {
        return AllocMmap(align, size);
    }

    type = SelectMempoolType(size);
    if (type < DFX_MEMPOOL_MIN_TYPE) {
        type = DFX_MEMPOOL_MIN_TYPE;
    }
    mempool = &(g_dfxAllocator.dfxMempoolBuf[type - DFX_MEMPOOL_MIN_TYPE]);
    return MempoolAlloc(mempool);
}

static void* DfxAlloc(size_t size)
{
    size_t realSize = size;
    if (size == 0) {
        realSize = 1;
    }
    return AllocImpl(ALIGN_SIZE, realSize);
}

static size_t GetChunkSize(void* ptr)
{
    if (ptr == nullptr) {
        printf("GetChunkSize Invalid ptr!\n");
        return 0;
    }
    PageInfo* page = GetPage(ptr);
    if (page == nullptr || page->tag.type < DFX_MEMPOOL_MIN_TYPE ||
        page->tag.type > DFX_MEMPOOL_MAX_TYPE) {
        printf("GetChunkSize Invalid page!\n");
        return 0;
    }
    if (page->tag.type == DFX_MMAP_TYPE) {
        return page->tag.mMapAllocSize - static_cast<size_t>(reinterpret_cast<uintptr_t>(ptr) -
            reinterpret_cast<uintptr_t>(page));
    }
    return g_dfxAllocator.dfxMempoolBuf[page->tag.type - DFX_MEMPOOL_MIN_TYPE].blockSize;
}

static void DfxFree(void* ptr)
{
    if (g_dfxAllocator.initFlag == 0) {
        return;
    }
    if (ptr == nullptr) {
        return;
    }
    PageInfo* page = GetPage(ptr);
    if (page == nullptr) {
        printf("DfxFree Invalid page!\n");
        return;
    }
    if (page->tag.type == DFX_MMAP_TYPE) {
        RemovePage(&(g_dfxAllocator.pageList), page);
        munmap(page, page->tag.mMapAllocSize);
    } else if (page->tag.type <= DFX_MEMPOOL_MAX_TYPE && page->tag.type >= DFX_MEMPOOL_MIN_TYPE) {
        DfxMempool* mempool = &(g_dfxAllocator.dfxMempoolBuf[page->tag.type - DFX_MEMPOOL_MIN_TYPE]);
        MempoolFree(mempool, ptr);
    }
}

static void* DfxRealloc(void* ptr, size_t size)
{
    if (g_dfxAllocator.initFlag == 0) {
        InitDfxAllocator();
    }
    if (ptr == nullptr) {
        return DfxAlloc(size);
    }
    if (size == 0) {
        return nullptr;
    }
    size_t oldsize = GetChunkSize(ptr);
    if (oldsize == 0) {
        return nullptr;
    }
    if (oldsize < size) {
        void* res = DfxAlloc(size);
        if (res) {
            if (memcpy_s(res, size, ptr, oldsize) != EOK) {
                printf("DfxRealloc memcpy fail\n");
            }
            DfxFree(ptr);
            return res;
        }
    }
    return ptr;
}

static void* HookMalloc(size_t size)
{
    return DfxAlloc(size);
}

static void* HookCalloc(size_t count, size_t size)
{
    return DfxAlloc(count * size);
}

static void* HookRealloc(void* ptr, size_t size)
{
    return DfxRealloc(ptr, size);
}

static void HookFree(void* ptr)
{
    return DfxFree(ptr);
}

void RegisterAllocator()
{
#ifndef DFX_ALLOCATE_ASAN
    (void)memcpy_s(&g_dfxCustomMallocDispatch, sizeof(g_dfxCustomMallocDispatch),
        &(__libc_malloc_default_dispatch), sizeof(__libc_malloc_default_dispatch));
#endif
    g_dfxCustomMallocDispatch.malloc = HookMalloc;
    g_dfxCustomMallocDispatch.calloc = HookCalloc;
    g_dfxCustomMallocDispatch.realloc = HookRealloc;
    g_dfxCustomMallocDispatch.free = HookFree;
#ifndef DFX_ALLOCATE_ASAN
    atomic_store_explicit(&ohos_malloc_hook_shared_library, -1, memory_order_seq_cst);
    atomic_store_explicit(&__hook_enable_hook_flag, true, memory_order_seq_cst);
    atomic_store_explicit(&__musl_libc_globals.current_dispatch_table,
        (volatile const long long)&g_dfxCustomMallocDispatch, memory_order_seq_cst);
#endif
}

void UnregisterAllocator(void)
{
#ifndef DFX_ALLOCATE_ASAN
    atomic_store_explicit(&ohos_malloc_hook_shared_library, 0, memory_order_seq_cst);
    atomic_store_explicit(&__hook_enable_hook_flag, false, memory_order_seq_cst);
    atomic_store_explicit(&__musl_libc_globals.current_dispatch_table, 0, memory_order_seq_cst);
#endif
}

DfxAllocator* GetDfxAllocator()
{
    return &g_dfxAllocator;
}

int IsDfxAllocatorMem(void* ptr)
{
    if (!ptr) {
        return 0;
    }
    if (GetPage(ptr)) {
        return 1;
    }
    return 0;
}
