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
#include "c/type_def.h"
#include <assert.h>
#ifdef TSAN_MODE
    #include <sanitizer/tsan_interface.h>
#endif

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

int co2_save_context(ffrt_fiber_t* ctx);

void co2_restore_context(ffrt_fiber_t* ctx);

static inline void co2_switch_context(ffrt_fiber_t* from, ffrt_fiber_t* to)
{
    if (co2_save_context(from) == 0) {
        #ifdef TSAN_MODE
            /* save current fiber to the source fiber */
            if (from->tsanFiber == nullptr) {
                from->tsanFiber = __tsan_get_current_fiber();
            }
            assert(from->tsanFiber && "src fiber is nullptr");
            if (to->tsanFiber == nullptr) {
                to->tsanFiber = __tsan_create_fiber(0);
            }
            assert(to->tsanFiber != from->tsanFiber);
            /* Switch to target fiber before the actual context switch
             * Note: Investigate if passing `__tsan_switch_to_fiber_no_sync`
             * and diabling happens before makes sense or not.
             * when passing 0 as a flag to the fiber switching we establish
             * a happens before relationship.
             */
            __tsan_switch_to_fiber(to->tsanFiber, 0);
        #endif
        co2_restore_context(to);
        #ifdef TSAN_MODE
        assert(to->tsanFiber == __tsan_get_current_fiber());
        #endif
    }
}
#ifdef  __cplusplus
}
#endif
#endif /* CO2_INT_H */
