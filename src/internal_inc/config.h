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

#include "sched/qos.h"

namespace ffrt {
constexpr int DEFAULT_MINCONCURRENCY = 4;
constexpr int INTERACTIVE_MAXCONCURRENCY = 4;
constexpr int DEFAULT_MAXCONCURRENCY = 8;
constexpr int DEFAULT_HARDLIMIT = 16;

class GlobalConfig {
public:
    GlobalConfig(const GlobalConfig&) = delete;

    GlobalConfig& operator=(const GlobalConfig&) = delete;

    ~GlobalConfig() {}

    static inline GlobalConfig& Instance()
    {
        static GlobalConfig cfg;
        return cfg;
    }

    void setCpuWorkerNum(enum qos qos, int num)
    {
        if (qos <= qos_inherit) {
            qos = qos_default;
        } else if (qos > qos_user_interactive) {
            qos = qos_user_interactive;
        }

        if ((num <= 0) || (num > DEFAULT_MAXCONCURRENCY)) {
            num = DEFAULT_MAXCONCURRENCY;
        }
        this->cpu_worker_num[static_cast<int>(qos)] = static_cast<size_t>(num);
    }

    int getCpuWorkerNum(enum qos qos)
    {
        return this->cpu_worker_num[static_cast<int>(qos)];
    }

    void setQosWorkers(const QoS &qos, int tid)
    {
        this->qos_workers[static_cast<int>(qos())].push_back(tid);
    }

    std::vector<std::vector<int>> getQosWorkers()
    {
        return qos_workers;
    }

private:
    GlobalConfig()
    {
        for (auto qos = QoS::Min(); qos < QoS::Max(); ++qos) {
            if (qos == (QoS::Max() - 1)) {
                this->cpu_worker_num[qos] = INTERACTIVE_MAXCONCURRENCY;
            } else {
                this->cpu_worker_num[qos] = DEFAULT_MAXCONCURRENCY;
            }
            std::vector<int> worker;
            this->qos_workers.push_back(worker);
        }
    }

    size_t cpu_worker_num[QoS::Max()];
    std::vector<std::vector<int>> qos_workers;
};
}

#endif /* GLOBAL_CONFIG_H */