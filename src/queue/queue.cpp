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
#include <cstdlib>
#include <cstring>
#include "dfx/log/ffrt_log_api.h"
#include "sched/qos.h"
#include "queue.h"

#ifdef  __cplusplus
extern "C" {
#endif
#ifdef FFRT_IO_TASK_SCHEDULER
/* 从队列的首部取出 */
void *queue_pophead(struct queue_s *queue)
{
    unsigned int head;
    unsigned int tail;
    void *res;

    while (1) {
        head = atomic_load(&queue->head);
        tail = atomic_load(&queue->tail);
        if (tail == head) {
            return nullptr;
        }
        res = queue->buf[head % queue->capacity];
        if (atomic_compare_exchange_weak(&queue->head, &head, head + 1)) {
            return res;
        }
    }
}

/*
 * 加入到队列的尾部
 * 返回值0表示OK 返回值ERROR_QUEUE_FULL表示队列已满插入失败
 */
int queue_pushtail(struct queue_s *queue, void *object)
{
    unsigned int head;
    unsigned int tail;
 
    head = atomic_load(&queue->head);
    tail = atomic_load(&queue->tail);
    if ((tail - head) < queue->capacity) {
        queue->buf[tail % queue->capacity] = object;
        atomic_store(&queue->tail, tail + 1);
        return 0;
    }
    return ERROR_QUEUE_FULL;
}

/*
 * 队列初始化
 * 头尾指针初始化为0，容量为传入的容量
 */
int queue_init(struct queue_s *queue, unsigned int capacity)
{
    if (capacity == 0) {
        return ERROR_QUEUE_ARG_INVALID;
    }
    queue->buf = (void **)malloc(sizeof(void *) * (capacity));
    if (queue->buf == nullptr) {
        FFRT_LOGE("queue malloc failed, size: %u", (sizeof(void *) * capacity));
        return ERROR_QUEUE_BUF_ALLOC_FAILED;
    }
    queue->capacity = capacity;
    atomic_store(&queue->head, (unsigned int)0);
    atomic_store(&queue->tail, (unsigned int)0);
    return 0;
}

void queue_destroy(struct queue_s *queue)
{
    if (queue->buf != nullptr) {
        free(queue->buf);
        queue->buf = nullptr;
    }
    queue->capacity = 0;
    atomic_store(&queue->head, (unsigned int)0);
    atomic_store(&queue->tail, (unsigned int)0);
}

/* 获取队列的长度 */
unsigned int queue_length(struct queue_s *queue)
{
    unsigned int head;
    unsigned int tail;

    head = atomic_load(&queue->head);
    tail = atomic_load(&queue->tail);
    return (tail - head);
}

/* 批量尾部插入 */
unsigned int queue_pushtail_batch(struct queue_s *queue, void *buf[], unsigned int buf_len)
{
    unsigned int head;
    unsigned int tail;
    unsigned int i;

    if (buf_len == 0) {
        return 0;
    }

    head = atomic_load(&queue->head);
    tail = atomic_load(&queue->tail);
    i = 0;
    while ((tail - head) < queue->capacity) {
        queue->buf[tail % queue->capacity] = buf[i];
        tail++;
        i++;
        if (i == buf_len) {
            break;
        }
    }
    atomic_store(&queue->tail, tail);
    return i;
}

/* 从首部批量获取 */
unsigned int queue_pophead_batch(struct queue_s *queue, void *buf[], unsigned int buf_len)
{
    unsigned int head;
    unsigned int tail;
    unsigned int pop_len;
    unsigned int i;

    if (buf_len == 0) {
        return 0;
    }

    while (1) {
        head = atomic_load(&queue->head);
        tail = atomic_load(&queue->tail);
        if (head == tail) {
            return 0;
        }
        pop_len = ((tail - head) > buf_len) ? buf_len : (tail - head);
        for (i = 0; i < pop_len; i++) {
            buf[i] = queue->buf[(head + i) % queue->capacity];
        }
        if (atomic_compare_exchange_weak(&queue->head, &head, head + pop_len)) {
            return pop_len;
        }
    }
}

/* 首部批量取出后将批量推入队列 */
unsigned int queue_pophead_pushtail_batch(struct queue_s *target_queue, struct queue_s *local_queue,
    unsigned int pop_len)
{
    if (pop_len == 0) {
        return 0;
    }
    unsigned int target_head;
    unsigned int target_tail;
    unsigned int local_head;
    unsigned int local_tail;
    unsigned int i;
    target_head = atomic_load(&target_queue->head);
    target_tail = atomic_load(&target_queue->tail);
    local_head = atomic_load(&local_queue->head);
    local_tail = atomic_load(&local_queue->tail);
    i = 0;
    while (((local_tail - local_head) < local_queue->capacity) && (target_tail != target_head)) {
        auto temp = queue_pophead(target_queue);
        if (temp == nullptr) {
            break;
        }
        local_queue->buf[local_tail % local_queue->capacity] = temp;
        local_tail++;
        i++;
        if (i == pop_len) {
            break;
        }
    }
    atomic_store(&local_queue->tail, local_tail);
    return i;
}

/* 本地队列首部批量取出一半元素后将元素批量推入全局队列尾部 */
void queue_pophead_to_gqueue_batch(struct queue_s* queue, unsigned int pop_len, int qos, queue_push_task_func_t func)
{
    if (pop_len == 0) {
        return;
    }
    unsigned int head;
    unsigned int tail;
    unsigned int i;

    head = atomic_load(&queue->head);
    tail = atomic_load(&queue->tail);
    i = 0;
    while ((tail != head) && i <= pop_len) {
        auto tmp = queue_pophead(queue);
        if (!func(tmp, qos)) {
            FFRT_LOGE("Submit IO task failed!");
            return;
        }
        i++;
    }
    return;
}

unsigned int queue_capacity(struct queue_s *queue)
{
    return queue->capacity;
}

unsigned int queue_prob(struct queue_s *queue)
{
    return queue_length(queue); // / (queue_capacity(queue) / 100);
}
#endif
#ifdef  __cplusplus
}
#endif
