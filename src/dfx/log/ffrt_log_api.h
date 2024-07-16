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

#ifdef OHOS_STANDARD_SYSTEM
#include <array>
#include <string_view>
#include "hilog/log.h"
#include <atomic>
#include <unistd.h>
#include <cstdint>
#include "dfx/bbox/fault_logger_fd_manager.h"
#else
#include "log_base.h"
#endif

#define FFRT_LOG_ERROR (0)
#define FFRT_LOG_WARN (1)
#define FFRT_LOG_INFO (2)
#define FFRT_LOG_DEBUG (3)
#define FFRT_LOG_LEVEL_MAX (FFRT_LOG_DEBUG + 1)

unsigned int GetLogId(void);

#ifdef OHOS_STANDARD_SYSTEM
template<size_t N>
constexpr auto convertFmtToPublic(const char(&str)[N])
{
    constexpr std::string_view fmtpub = "{public}";
    std::array<char, (N / 2) * fmtpub.size() + N> res{};
    for (size_t i = 0, j = 0; i < N; ++i) {
        res[j++] = str[i];
        if (str[i] != '%') {
            continue;
        }

        if (str[i + 1] != '%' && str[i + 1] != '{') {
            for (size_t k = 0; k < fmtpub.size(); ++k) {
                res[j++] = fmtpub[k];
            }
        } else {
            res[j++] = str[i + 1];
            i += 1;
        }
    }

    return res;
}

#ifdef HILOG_FMTID
#define HILOG_IMPL_STD_ARRAY(type, level, fmt, ...) \
    do { \
        constexpr HILOG_FMT_IN_SECTION static auto hilogFmt = fmt; \
        FmtId fmtid{ HILOG_UUID, HILOG_FMT_OFFSET(hilogFmt.data()) }; \
        HiLogPrintDict(type, level, 0xD001719, "ffrt", &fmtid, hilogFmt.data(), ##__VA_ARGS__); \
    } while (0)
#else
#define HILOG_IMPL_STD_ARRAY(type, level, fmt, ...) \
    do { \
        HiLogPrint(type, level, 0xD001719, "ffrt", fmt.data(), ##__VA_ARGS__); \
    } while (0)
#endif

#if (FFRT_LOG_LEVEL >= FFRT_LOG_DEBUG)
#define FFRT_LOGD(format, ...) \
    do { \
        constexpr auto fmtPub = convertFmtToPublic("%u:%s:%d " format); \
        HILOG_IMPL_STD_ARRAY(LOG_CORE, LOG_DEBUG, fmtPub, GetLogId(), __func__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define FFRT_LOGD(format, ...)
#endif

#if (FFRT_LOG_LEVEL >= FFRT_LOG_INFO)
#define FFRT_LOGI(format, ...) \
    do { \
        constexpr auto fmtPub = convertFmtToPublic("%u:%s:%d " format); \
        HILOG_IMPL_STD_ARRAY(LOG_CORE, LOG_INFO, fmtPub, GetLogId(), __func__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define FFRT_LOGI(format, ...)
#endif

#if (FFRT_LOG_LEVEL >= FFRT_LOG_WARN)
#define FFRT_LOGW(format, ...) \
    do { \
        constexpr auto fmtPub = convertFmtToPublic("%u:%s:%d " format); \
        HILOG_IMPL_STD_ARRAY(LOG_CORE, LOG_WARN, fmtPub, GetLogId(), __func__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
#define FFRT_LOGW(format, ...)
#endif

#define FFRT_LOGE(format, ...) \
    do { \
        constexpr auto fmtPub = convertFmtToPublic("%u:%s:%d " format); \
        HILOG_IMPL_STD_ARRAY(LOG_CORE, LOG_ERROR, fmtPub, GetLogId(), __func__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#else
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
#endif

#ifdef OHOS_STANDARD_SYSTEM
#define FFRT_BBOX_LOG(format, ...) \
    do { \
        FFRT_LOGE(format, ##__VA_ARGS__); \
        FaultLoggerFdManager::Instance().WriteFaultLogger(format, ##__VA_ARGS__); \
    } while (0)
#else
#define FFRT_BBOX_LOG(format, ...) FFRT_LOGE(format, ##__VA_ARGS__)
#endif

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
