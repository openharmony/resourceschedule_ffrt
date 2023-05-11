#!/usr/bin/env python3
# coding=utf-8
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

import os
import sys
import logging
import pandas
from matplotlib import pyplot as plt

# Gets the minimum value greater than 1


def get_speedup_1_duration(data):
    p = 0
    for i in range(len(data)):
        if(data['speedup'][i] > 1 and data['speedup'][i] < data['speedup'][p]):
            p = i
    return data['duration'][p]


def plot_current_only(argv):
    current_data = argv[1]
    fig = plt.figure()

    current = pandas.read_csv(current_data, delim_whitespace=True)
    min_duration = get_speedup_1_duration(current)

    plt.plot(current['duration'], current['speedup'],
             color='r', marker='o', linestyle='-', label=''.join(['speedup=1@duration=',  str(min_duration)]))

    plt.legend(loc='best')
    plt.xlabel('task_duration(us)')
    plt.ylabel('speedup')
    plt.title('ffrt speedup test', fontsize=20)
    plt.savefig("speedup.jpg")


def plot_current_vs_base(argv):
    current_data = argv[1]
    if len(argv) <= 2:
        logging.warning('no base data')
        return
    else:
        base_data = argv[2]

    fig = plt.figure()

    base = pandas.read_csv(base_data, delim_whitespace=True)
    base_min_duration = get_speedup_1_duration(base)

    plt.plot(base['duration'], base['speedup'], color='b',
             marker='s', linestyle='-.', label=''.join(['base speedup=1@duration=',  str(base_min_duration)]))

    current = pandas.read_csv(current_data, delim_whitespace=True)
    current_min_duration = get_speedup_1_duration(current)

    plt.plot(current['duration'], current['speedup'],
             color='r', marker='o', linestyle='-', label=''.join(['curren speedup=1@duration=',  str(current_min_duration)]))

    plt.legend(loc='best')
    plt.xlabel('task_duration(us)')
    plt.ylabel('speedup')
    plt.title('ffrt speedup test', fontsize=20)
    plt.savefig("speedup_vs_base.jpg")


if __name__ == '__main__':
    plot_current_vs_base(sys.argv)
    plot_current_only(sys.argv)
