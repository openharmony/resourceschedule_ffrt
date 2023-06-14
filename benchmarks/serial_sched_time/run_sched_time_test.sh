#!/bin/bash
# Copyright (c) 2023 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

cd $(dirname $0)/../
benchmark_dir=$(pwd)
mkdir -p output

cd ..
rm -rf build/
mkdir build && cd build

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DFFRT_BENCHMARKS=ON \
    -DBENCHMARKS_SERIAL_SCHED_TIME=ON \

make -j ffrt
make -j serial_sched_time_test
FFRT_LOG_LEVEL=0 ./benchmarks/serial_sched_time/serial_sched_time_test |tee serial_sched_time_test.log

rm -f ${benchmark_dir}/output/serial_sched_time_test.csv
echo duration sched_time >> ${benchmark_dir}/output/serial_sched_time_test.csv
# use spaces and ':' to split log lines
awk -F '[ :]' '{print $6,$12}' serial_sched_time_test.log >>${benchmark_dir}/output/serial_sched_time_test.csv

cd ${benchmark_dir}/output
${benchmark_dir}/serial_sched_time/plot.py ${benchmark_dir}/output/serial_sched_time_test.csv ${benchmark_dir}/serial_sched_time/base.csv
