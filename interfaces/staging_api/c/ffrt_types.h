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

#ifndef FFRT_TYPES_H
#define FFRT_TYPES_H

typedef void(*ffrt_function_t)(void*);

typedef enum {
    NONE = -1,
    FFRT_DEV_HTS,
    FFRT_DEV_FFTS,
    FFRT_DEV_GPU,
    FFRT_DEV_NPU,
    FFRT_DEV_RESV, // NPU TSCPU预留
    FFRT_DEV_ISP,
    FFRT_DEV_DSS,
    FFRT_DEV_VENC,
    FFRT_DEV_VDEC,
    FFRT_DEV_AUDIO,
    FFRT_DEV_SENSERHUB,
    FFRT_DEV_IPP,
    FFRT_DEV_AAE,
    FFRT_DEV_DPA,
    FFRT_DEV_DFA,
    ffrt_DEV_CPU
} ffrt_dev_type;

/**
 * @brief Defines a hcs task
 *
 */
typedef struct {
    ffrt_function_t exec;
    ffrt_function_t destory;
    void* args;
} ffrt_callable_t;

typedef struct {
    ffrt_dev_type dev_type;
    ffrt_callable_t run;
    ffrt_callable_t pre_run;
    ffrt_callable_t post_run;
} ffrt_hcs_task_t;

enum ffrt_device_property {
    FFRT_HW_DYNAMIC_CPU = 1,        // b01
    FFRT_HW_DYNAMIC_XPU_NORMAL = 2, // b10
    FFRT_HW_DYNAMIC_XPU_EVENT = 4,  // b100
    FFRT_HW_STATIC_EVENT = 8,       // b1000
    FFRT_HW_STATIC_NORMAL = 16,     // b10000
};

enum err_type {
    FFRT_HW_EXEC_SUCC,
    FFRT_HW_EXEC_FAIL,
    FFRT_HW_PARAM_WRONG,
    FFRT_HW_ERR_MAX
};
#endif // FFRT_TYPES_H