/*
* Copyright (c) 2023 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, sofware
* distributed under the Licenses is distributed on an "AS IS" BASIS,
* WITHOUT WARRNATIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limations under the License.
*/

#include <gtest/gtest.h>
#include <cstdlib>
#include <mutex>
#include <cmath>
#include <chrono>
#include <thread>
#include <fstream>
#include <random>
#include <algorithm>
#include "util.h"
#include "ffrt_inner.h"
#include "eu/co_routine.h"
