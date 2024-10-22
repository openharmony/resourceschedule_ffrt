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
#ifndef TOKEN_H
#define TOKEN_H
#include <atomic>
#include <cstdio>
#include <stdlib.h>
#include <new>

namespace ffrt {

class Token {
public:
    using token_value_t = std::atomic<unsigned int>;

    Token() = delete;
    Token(unsigned int init)
    {
        count.store(init);
    }

    inline bool try_acquire()
    {
        bool ret = true;
        for (; ;) {
            unsigned int v = count.load(std::memory_order_relaxed);
            if (v == 0) {
                ret = false;
                break;
            }
            if (count.compare_exchange_strong(v, v - 1, std::memory_order_acquire, std::memory_order_relaxed)) {
                break;
            }
        }
        return ret;
    }

    inline void release()
    {
        count.fetch_add(1);
    }

    unsigned int load()
    {
        return count.load();
    }
private:
    token_value_t count;
};
}
#endif