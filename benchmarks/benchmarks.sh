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
build() {
    cd ${benchmarks_path}/../
    rm -rf build/
    mkdir build
    cd build
    cmake .. -DFFRT_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
    make -j ffrt
    make -j
}

run_all() {
    task_compute_time_tbl=(0 5 10 15 20 25 30 35 40 50 60 70 80 90 100 120 140 160 180 200)
    export FIB_NUM=20
    export REPEAT=$2
    is_first=1
    for task_compute_time in ${task_compute_time_tbl[@]}; do
        export COMPUTE_TIME_US=$task_compute_time
        {
            echo "export FIB_NUM=$FIB_NUM; export COMPUTE_TIME_US=$COMPUTE_TIME_US; REPEAT=$REPEAT"
            ./benchmarks/fork_join
            ./benchmarks/fib
            ./benchmarks/face_story
            ./benchmarks/base
        } | tee $output_dir/perf_$1_${task_compute_time}.log
        if [ "$is_first" == "1" ]; then
            echo repeat,task_compute_time,\
            $(grep -w us $output_dir/perf_$1_${task_compute_time}.log  | awk '{print $2}' | tr '\n' ',') >> $output_dir/perf_$1.csv
            is_first=0
        fi
        echo $REPEAT,$task_compute_time,\
        $(grep -w us $output_dir/perf_$1_${task_compute_time}.log  | awk '{print $3}' | tr '\n' ',') >> $output_dir/perf_$1.csv
    done | tee $output_dir/perf_$1_.log
    touch $output_dir/perf_$1.csv
}

# create dir
cd $(dirname $0)
benchmarks_path=$(pwd)
stamp=$(git log --date=format:'%Y_%m_%d_%H_%M_%S' -1 | grep Date | sed -E 's/[^0-9]*([0-9_]*)/\1/g')
output_dir=${benchmarks_path}/output/tag_$stamp
rm -rfd $output_dir
mkdir -m 777 -p $output_dir

# build
export TOOLCHAIN_PATH=../../../../../prebuilts/clang/host/linux-x86/clang-r416183b1
export PATH=$TOOLCHAIN_PATH/bin:$PATH
export LD_LIBRARY_PATH=$TOOLCHAIN_PATH/lib64:$LD_LIBRARY_PATH
build

# benchmark
export REPEAT=1;export PREHOT_FFRT=1;export FFRT_LOG_LEVEL=0
exec_times=${1:-'1'}

echo 1 > ../ffrt.cfg  && run_all thread1 ${exec_times}
echo 8 > ../ffrt.cfg  && run_all thread8 ${exec_times}

cp "$benchmarks_path/base.csv" "$output_dir"
MPLBACKEND=svg "$benchmarks_path/plot.py" "$output_dir" "benchmark_${stamp}.svg"
