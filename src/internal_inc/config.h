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

#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include "qos.h"
#include "types.h"

namespace ffrt {
constexpr unsigned int DEFAULT_GLOBAL_HARDLIMIT = 96;
constexpr unsigned int DEFAULT_PARAMS_VALUE = 0Xffffffff;

constexpr unsigned int DEFAULT_MAXCONCURRENCY = 8;
constexpr unsigned int MAX_MAXCONCURRENCY = 12;
constexpr unsigned int DEFAULT_HARDLIMIT = 44;
constexpr unsigned int DEFAULT_SINGLE_NUM = 8;

constexpr unsigned int DEFAULT_GLOBAL_RESERVE_NUM = 24;
constexpr unsigned int DEFAULT_LOW_RESERVE_NUM = 12;
constexpr unsigned int DEFAULT_HIGH_RESERVE_NUM = 12;
constexpr unsigned int GLOBAL_QOS_MAXNUM = 256;

class QosWorkerConfig {
public:
    struct FfrtQosWorkerNumCfg {
        unsigned int hardLimit = DEFAULT_HARDLIMIT;
        unsigned int maxConcurrency = DEFAULT_MAXCONCURRENCY;
        unsigned int reserveNum = DEFAULT_SINGLE_NUM;
    };

    QosWorkerConfig(int workerNum)
    {
        mQosWorkerCfg.resize(workerNum);
    }
    QosWorkerConfig(const QosWorkerConfig&) = delete;
    QosWorkerConfig& operator=(const QosWorkerConfig&) = delete;
    ~QosWorkerConfig() {}

    unsigned int GetGlobalMaxWorkerNum() const
    {
        unsigned int ret = 0;
        ret += mLowQosReserveWorkerNum;
        ret += mHighQosReserveWorkerNum;
        ret += mGlobalReserveWorkerNum;
        for (const auto &tmpStru : mQosWorkerCfg) {
            ret += tmpStru.reserveNum;
        }
        return ret;
    }

    std::vector<FfrtQosWorkerNumCfg> mQosWorkerCfg;
    unsigned int mLowQosReserveWorkerNum = DEFAULT_LOW_RESERVE_NUM;
    unsigned int mHighQosReserveWorkerNum = DEFAULT_HIGH_RESERVE_NUM;
    unsigned int mGlobalReserveWorkerNum = DEFAULT_GLOBAL_RESERVE_NUM;
};
}

#endif /* GLOBAL_CONFIG_H */
