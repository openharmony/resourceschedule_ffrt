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

#ifndef CO2_INT_H
#define CO2_INT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__aarch64__)
#define FFRT_REG_NR 22
#define FFRT_REG_LR 11
#define FFRT_REG_SP 13
#elif defined(__arm__)
#define FFRT_REG_NR 64
#define FFRT_REG_LR 1
#define FFRT_REG_SP 0
#elif defined(__x86_64__)
#define FFRT_REG_NR 8
#define FFRT_REG_LR 7
#define FFRT_REG_SP 6
#elif defined(__riscv) && __riscv_xlen == 64
// https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/riscv/bits/setjmp.h;h=5dd7fa0120ab37c9ec5c4a854792c0935b9eddc1;hb=HEAD
#if defined __riscv_float_abi_double
#define FFRT_REG_NR 26
#else
#define FFRT_REG_NR 14
#endif
#define FFRT_REG_LR 0
#define FFRT_REG_SP 13
#else
#error "Unsupported architecture"
#endif

struct co2_context {
    uintptr_t regs[FFRT_REG_NR];
};

int co2_save_context(struct co2_context* ctx);

void co2_restore_context(struct co2_context* ctx);

int co2_init_context(struct co2_context* ctx, void (*func)(void*), void* arg, void* stack, size_t stack_size);

#ifdef __cplusplus
}
#endif
#endif /* CO2_INT_H */
