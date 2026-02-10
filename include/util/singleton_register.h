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

#ifndef UTIL_SINGLETON_H
#define UTIL_SINGLETON_H

#include "util/cb_func.h"
namespace ffrt {
template <typename T>
class SingletonRegister {
public:
    static SingletonRegister<T>& Instance_()
    {
        if unlikely(ins_ == nullptr) {
            CreateSingletonRegister();
        }
        return *ins_;
    }

    static inline T& Instance()
    {
        return Instance_().cb_();
    }

    static void RegistInsCb(typename SingleInsCB<T>::Instance &&cb)
    {
        Instance_().cb_ = std::move(cb);
    }

private:
    static FFRT_NOINLINE void CreateSingletonRegister()
    {
        static SingletonRegister<T> ins;
        ins_ = &ins;
    }

    typename SingleInsCB<T>::Instance cb_;
    static inline SingletonRegister<T>* ins_ = nullptr;
};
} // namespace ffrt
#endif