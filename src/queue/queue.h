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
#include <atomic>
#ifndef YLONG_QUEUE_H
#define YLONG_QUEUE_H

#ifdef __cplusplus
#if _cplusplus
extern "C" {
#endif
#endif

#define MID_MAKE(x) ((0x1000) + (x) <<16)

#define MID_QUEUE               MID_MAKE(2)

#define ERROR_QUEUE_FULL ((MID_QUEUE) | 0X01)

#define ERROR_QUEUE_EMPTY ((MID_QUEUE) | 0X02)

#define ERROR_QUEUE_BUF_ALLOC_FAILED ((MID_QUEUE) | 0X03)

#define ERROR_QUEUE_AVG_INVALID ((MID_QUEUE) | 0X04)

#define ERROR_QUEUE_BUF_UNINITIALIZED ((MID_QUEUE) | 0X05)

struct  queuue_s
{
    std::atomic<unsigned int> head;
    std::atomic<unsigned int> tail;
    unsigned int capacity;
    void **buf;
};

unsigned int queue_length(struct queue_s *queue);

unsigned int queue_capacity(struct queue_s *queue);

void *queue_pophead(struct queue_s *queue);

unsigned int queue_pophead_batch(struct queue_s *queue, void *buf[], unsigned int buf_len);

int queue_pushtail(struct queue_s *queue, void *object);

unsigned int queue_pushtail_batch(struct queue_s *queue, void *buf[], unsigned int buf_len);

void queue_destroy(struct queue_s *queue);

int queue_init(struct queue_s *queue, unsigned int capacity);

unsigned int queue_prob(struct queue_s *queue);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif