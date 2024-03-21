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

#ifndef __FFRT_LOG_API_H__
#define __FFRT_LOG_API_H__

#include "log_base.h"

#if (FFRT_LOG_LEVEL >= FFRT_LOG_DEBUG)
#define FFRT_LOGD(format, ...) FFRT_LOG(FFRT_LOG_DEBUG, format, ##__VA_ARGS__)
#else
#define FFRT_LOGD(format, ...)
#endif

#if (FFRT_LOG_LEVEL >= FFRT_LOG_INFO)
#define FFRT_LOGI(format, ...) FFRT_LOG(FFRT_LOG_INFO, format, ##__VA_ARGS__)
#else
#define FFRT_LOGI(format, ...)
#endif

#if (FFRT_LOG_LEVEL >= FFRT_LOG_WARN)
#define FFRT_LOGW(format, ...) FFRT_LOG(FFRT_LOG_WARN, format, ##__VA_ARGS__)
#else
#define FFRT_LOGW(format, ...)
#endif

#define FFRT_LOGE(format, ...) FFRT_LOG(FFRT_LOG_ERROR, format, ##__VA_ARGS__)

#define FFRT_COND_DO_ERR(cond, expr, format, ...) \
    if (cond) {                                   \
        FFRT_LOGE(format, ##__VA_ARGS__);         \
        {                                         \
            expr;                                 \
        }                                         \
    }

// Do not use this Marco directly
#define COND_RETURN_(COND, ERRCODE, ...) \
    if ((COND)) { \
        FFRT_LOGE(__VA_ARGS__); \
        return ERRCODE; \
    }

#define FFRT_COND_RETURN_ERROR(COND, ERRCODE, ...) \
    COND_RETURN_((COND), ERRCODE, ##__VA_ARGS__)

#define FFRT_COND_RETURN_VOID(COND, ...) \
    if ((COND)) { \
        FFRT_LOGE(__VA_ARGS__); \
        return; \
    }

// Do not use this Marco directly
#define COND_GOTO_WITH_ERRCODE_(COND, LABEL, ERROR, ERRCODE, ...) \
    if ((COND)) { \
        FFRT_LOGE(__VA_ARGS__); \
        ERROR = (ERRCODE); \
        goto LABEL; \
    }

#define FFRT_COND_GOTO_ERROR(COND, LABEL, ERROR, ERRCODE, ...) \
    COND_GOTO_WITH_ERRCODE_((COND), LABEL, ERROR, ERRCODE, ##__VA_ARGS__)

#define FFRT_UNUSED(expr) \
    do { \
        (void)(expr); \
    } while (0)

#endif // __FFRT_LOG_API_H__
