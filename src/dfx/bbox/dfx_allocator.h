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
#ifndef DFX_ALLOCATOR_H
#define DFX_ALLOCATOR_H

#include <sys/types.h>

constexpr int DFX_MEMPOOLS_NUM = 7;
constexpr int DFX_MEMPOOL_TAG_SIZE = 4;

typedef struct DfxMempoolSt DfxMempool;

typedef struct DfxBlockInfo {
    struct DfxBlockInfo* next;
    size_t freeBlocksCnt;
} BlockInfo;

typedef struct DfxPageTag {
    char tagInfo[DFX_MEMPOOL_TAG_SIZE];
    uint32_t type;
    union {
        size_t mMapAllocSize;
        DfxMempool* mempool;
    };
} PageTag;

typedef struct DfxPageInfo {
    PageTag tag;
    struct DfxPageInfo* prev;
    struct DfxPageInfo* next;
    BlockInfo* freeBlockList;
    size_t freeBlocksCnt;
} PageInfo;

struct DfxMempoolSt {
    uint32_t type;
    size_t blockSize;
    size_t blocksPerPage;
    size_t freePagesCnt;
    PageInfo* pageList;
};

typedef struct DfxAllocatorSt {
    int initFlag;
    PageInfo* pageList;
    DfxMempool dfxMempoolBuf[DFX_MEMPOOLS_NUM];
} DfxAllocator;

/**
 * @brief Get allocator
 *   an independent memory allocator
 *   to perform memory operations
 * @return DfxAllocator addr of allcator for get some info
*/
DfxAllocator* GetDfxAllocator(void);

/**
 * @brief Change use custom allocator
 *   an independent memory allocator
 *   to perform memory operations
*/
void RegisterAllocator(void);

/**
 * @brief restore default allocator
 *   an independent memory allocator
 *   to perform memory operations
*/
void UnregisterAllocator(void);

/**
 * @brief IsDfxAllocatorMem
 *   an independent memory allocator
 *   to perform memory operations
 * @return 1 : addr belong DfxMem, 0 :not
*/
int IsDfxAllocatorMem(void* ptr);
#endif