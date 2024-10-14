/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef FFRT_STATIC_GRAPH_H
#define FFRT_STATIC_GRAPH_H

#include <stdint.h>
#include "ffrt_types.h"

#define NODE_NUM_MAX 3
#define NODE_NAME_LEN_MAX 20
#define API_ATTRIBUTE(attr) __attribute__(attr)

typedef struct {
    unsigned int input_node_num;
    char input_node_name[NODE_NUM_MAX][NODE_NAME_LEN_MAX];
    ffrt_dev_type input_dev_type[NODE_NUM_MAX];
    unsigned int src_event[NODE_NUM_MAX];
    unsigned int output_node_num;
    char out_node_name[NODE_NUM_MAX][NODE_NAME_LEN_MAX];
    ffrt_dev_type output_dev_type[NODE_NUM_MAX];
    unsigned int dst_event[NODE_NUM_MAX];
    unsigned int src_repeate;
    unsigned int dst_repeate;
} ffrt_static_node;

typedef struct {
    unsigned int num;
    ffrt_static_node* node;
} ffrt_static_graph_user;

#define API_ATTRIBUTE __attribute__(attr)

#ifdef __cplusplus
extern "C" {
#endif
#define GRAPH_NAME_LEN_MAX 50
ffrt_static_graph_user* ffrt_graph_register(const char *name);
int ffrt_graph_unregister(const char *name);
int ffrt_graph_enable(const char *name);
int ffrt_graph_disable(const char *name);
int ffrt_alloc_event(uint16_t *events, uint32_t num, uint32_t* event_handle);
int ffrt_free_event(uint32_t event_handle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // FFRT_STATIC_GRAPH_H