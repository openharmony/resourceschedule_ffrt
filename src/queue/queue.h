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
 *
 */

#ifndef YLONG_QUEUE_H
#define YLONG_QUEUE_H

#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif
#ifdef FFRT_IO_TASK_SCHEDULER
/**
 * @ingroup mid
 * @brief 生成模块号
 * @param  x   [IN]  模块号。
 */
#define MID_MAKE(x) ((0x1000 + (x)) << 16)

#define MID_QUEUE                MID_MAKE(2)
/**
 * @brief 0x10020001
 * 插入时队列已满
 */
#define ERROR_QUEUE_FULL ((MID_QUEUE) | 0x01)

/**
 * @brief 0x10020002
 * 取出时队列为空
 */
#define ERROR_QUEUE_EMPTY ((MID_QUEUE) | 0x02)

/**
 * @brief 0x10020003
 * 初始化时，queue->buf申请内存失败
 */
#define ERROR_QUEUE_BUF_ALLOC_FAILED ((MID_QUEUE) | 0x03)

/**
 * @brief 0x10020004
 * 函数传入的入参非法
 */
#define ERROR_QUEUE_ARG_INVALID ((MID_QUEUE) | 0x04)

/**
 * @brief 0x10020005
 * queue->buf为空，queue未初始化
 */
#define ERROR_QUEUE_BUF_UNINITIALIZED ((MID_QUEUE) | 0x05)


/**
 * @ingroup queue
 * @struct queue_s
 * @brief queue结构体 \n
 * 定义了queue的头尾、容量和队列指针
 */
struct queue_s {
    std::atomic<unsigned int> head;
    std::atomic<unsigned int> tail;
    unsigned int capacity;
    void **buf;
};

/**
 * @ingroup queue
 * @brief 获取队列长度。
 * @par 描述：获取指定队列的长度并返回。
 * @attention 入参queue不得为NULL，否则段错误，且queue必须已经初始化
 * @param  queue   [IN]  要获取长度的指定队列。
 * @retval 成功返回队列长度，失败返回0
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see
 */
unsigned int queue_length(struct queue_s *queue);

/**
 * @ingroup queue
 * @brief 获取队列的容量。
 * @par 描述：获取指定队列的容量并返回。
 * @attention 入参queue不得为NULL，否则段错误，且queue必须已经初始化
 * @param  queue   [IN]  要获取容量的指定队列。
 * @retval 队列的容量。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see
 */
unsigned int queue_capacity(struct queue_s *queue);

/**
 * @ingroup queue
 * @brief 从队列的首部取出元素。
 * @par 描述：取出队列首部元素并返回指向此元素的指针。
 * @attention 入参queue不得为NULL，否则段错误，且queue必须已经初始化
 * @param  queue   [IN]  要取出头部的队列。
 * @retval 指向首部元素的指针，若队列为空则返回NULL。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see
 */
void *queue_pophead(struct queue_s *queue);

/**
 * @ingroup queue
 * @brief 从队列首部批量取出元素。
 * @par 描述：批量取出队列首部元素并返回取出元素的数量。
 * @attention 入参queue,buf不得为NULL，否则段错误，且queue必须已经初始化
 * @param  queue   [IN]  要批量取出头部元素的队列。
 * @param  buf   [IN]  将取出的元素放入此buffer。
 * @param  buf_len   [IN]  buffer的长度
 * @retval 取出的元素数量，最大为buf_len。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see queue_pophead
 */
unsigned int queue_pophead_batch(struct queue_s *queue, void *buf[], unsigned int buf_len);

/**
 * @ingroup queue
 * @brief 将元素推入队列尾部。
 * @par 描述：将指定元素推入队列的尾部。
 * @attention 入参queue不得为NULL，否则段错误，且queue必须已经初始化，仅支持单线程插入
 * @param  queue   [IN]  要被推入元素的队列。
 * @param  object   [IN]  要推入队列的元素。
 * @retval 错误码，0为成功， 失败返回错误码。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see
 */
int queue_pushtail(struct queue_s *queue, void *object);

/**
 * @ingroup queue
 * @brief 将元素批量推入队列尾部。
 * @par 描述：将缓存区里的元素批量推入队列的尾部。
 * @attention 入参queue,buf不得为NULL，否则段错误，且queue必须已经初始化，仅支持单线程插入
 * @param  queue   [IN]  要被推入元素的队列。
 * @param  buf   [IN]  包含要被推入队列尾部的元素的buffer。
 * @param  buf_len   [IN]  buffer的长度
 * @retval 成功返回被推入队列尾部的元素数量，最大为buf_len。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see queue_pushtail
 */
unsigned int queue_pushtail_batch(struct queue_s *queue, void *buf[], unsigned int buf_len);

/**
 * @ingroup queue
 * @brief 从队列A首部批量取出元素后将元素批量推入队列B尾部。
 * @par 描述：批量取出队列A首部元素推入队列B的尾部并返回取出元素的数量。
 * @attention 入参target_queue,local_queue不得为NULL，否则段错误，且queue必须已经初始化，仅支持单线程插入
 * @param  target_queue   [IN]  要批量取出头部元素的队列。
 * @param  local_queue   [IN]  要被推入元素的队列。
 * @param  buf   [IN]  包含要被推入队列尾部的元素的buffer。
 * @param  pop_len   [IN]  pop的长度
 * @retval 成功返回被推入队列尾部的元素数量，最大为pop_len。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see queue_pophead_pushtail_batch
 */
unsigned int queue_pophead_pushtail_batch(struct queue_s *target_queue, struct queue_s *local_queue,
    unsigned int pop_len);

/**
 * @ingroup queue
 * @brief 从本地队列首部批量取出一半元素后将元素批量推入全局队列尾部。
 * @par 描述：批量取出队列A首部元素推入队列B的尾部并返回取出元素的数量。
 * @attention 入参queue不得为NULL，否则段错误，且queue必须已经初始化，仅支持单线程插入
 * @param  queue   [IN]  要批量取出头部元素的队列。
 * @param  pop_len   [IN]  pop的长度
 * @param  qos   [IN]  要被推入元素的优先级。
 * @param  func   [IN]  被封装的推入队列B的操作。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see queue_pophead_to_gqueue_batch
 */
typedef bool (*queue_push_task_func_t)(void* task, int qos);
void queue_pophead_to_gqueue_batch(struct queue_s* queue, unsigned int pop_len, int qos, queue_push_task_func_t func);

/**
 * @ingroup queue
 * @brief 销毁队列
 * @par 描述：释放队列资源，销毁后不能访问，只释放结构体内数组，队列结构体需用户自己释放。
 * @attention 入参queue不得为NULL，否则段错误，且queue必须已经初始化
 * @param  queue   [IN]  要被销毁的队列。
 * @retval 无
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see
 */
/* 销毁队列，销毁后不能再访问 */
void queue_destroy(struct queue_s *queue);

/**
 * @ingroup queue
 * @brief 初始化队列
 * @par 描述：初始化指定队列。
 * @attention 入参queue不得为NULL，否则段错误， capacity非0
 * @param  queue   [IN]  将被初始化的队列
 * @param  capacity   [IN]  新建队列的容量。
 * @retval 成功返回0，失败返回错误码。
 * @par 依赖：无。
 * @li queue.h：该接口声明所在的文件。
 * @see
 */
int queue_init(struct queue_s *queue, unsigned int capacity);

/**
 * 估计偷取概率，取值为0-100
 */
unsigned int queue_prob(struct queue_s *queue);
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif
#endif /* _QUEUE_H */

