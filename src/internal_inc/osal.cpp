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

#include <string>

std::string GetEnv(const char* name)
{
#ifdef _MSC_VER
    char* r = nullptr;
    size_t len = 0;

    if (_dupenv_s(&r, &len, name) == 0 && r != nullptr) {
        std::string val(r, len);
        std::free(r);
        return val;
    }
    return "";

#else
    char* val = std::getenv(name);
    if (val == nullptr) {
        return "";
    }
    return val;
#endif
}
