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
#ifndef FFRT_QOS_DEF_H
#define FFRT_QOS_DEF_H

typedef enum {
    ffrt_inner_qos_begin = 200,
    ffrt_inner_qos_background = ffrt_inner_qos_begin,
    ffrt_inner_qos_less_favorable,
    ffrt_inner_qos_normal,
    ffrt_inner_qos_more_favorable,
    ffrt_inner_qos_further_favorable,
    ffrt_inner_qos_foreground,
    ffrt_inner_qos_urgent_display,
    ffrt_inner_qos_highest,
    ffrt_inner_qos_highest_bind,
    ffrt_inner_qos_vip,
    ffrt_inner_qos_vip_bind,
    ffrt_inner_qos_rt,
    ffrt_inner_qos_max = ffrt_inner_qos_rt,
} ffrt_inner_qos_t;

typedef enum {
    ffrt_qos_custom_begin = 300,
    ffrt_qos_defined_ive = ffrt_qos_custom_begin,
    ffrt_qos_custom_max = ffrt_qos_defined_ive,
} ffrt_custom_qos_t;

#ifdef __cplusplus
namespace ffrt {
enum qos_inner {
    qos_inner_begin = ffrt_inner_qos_begin,
    qos_inner_background = qos_inner_begin,
    qos_inner_less_favorable = ffrt_inner_qos_less_favorable,
    qos_inner_normal = ffrt_inner_qos_normal,
    qos_inner_more_favorable = ffrt_inner_qos_more_favorable,
    qos_inner_further_favorable = ffrt_inner_qos_further_favorable,
    qos_inner_foreground = ffrt_inner_qos_foreground,
    qos_inner_urgent_display = ffrt_inner_qos_urgent_display,
    qos_inner_highest = ffrt_inner_qos_highest,
    qos_inner_highest_bind = ffrt_inner_qos_highest_bind,
    qos_inner_vip = ffrt_inner_qos_vip,
    qos_inner_vip_bind = ffrt_inner_qos_vip_bind,
    qos_inner_rt = ffrt_inner_qos_rt,
    qos_inner_max = ffrt_inner_qos_max
};

enum qos_custom {
    qos_custom_begin = ffrt_qos_custom_begin,
    qos_defined_ive = ffrt_qos_defined_ive,
    qos_custom_max = ffrt_qos_custom_max,
};
}
#endif
#endif /* FFRT_QOS_DEF_H */