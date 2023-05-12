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


def plot_current_olny(argv):
    current_data = argv[1]
    fig = plt.figure()

    current = pandas.read_csv(current_data, delim_whitespace=True)
    plt.plot(current['duration'], current['sched_time'],
             color='r', marker='o', linestyle='-', label='current')

    avg = sum(current['sched_time']) / len(current['sched_time'])

    plt.xlabel('task_duration(us)')
    plt.ylabel('sched_time(us)')
    plt.title(''.join(
        ['ffrt sched_time test [avg:', str(round(avg, 2)), ' us]']), fontsize=20)
    plt.savefig("serial_sched_time.jpg")


def plot_current_vs_base(argv):
    current_data = argv[1]
    if len(argv) <= 2:
        logging.warning('no base data')
        return
    else:
        base_data = argv[2]
    fig = plt.figure()

    base = pandas.read_csv(base_data, delim_whitespace=True)
    base_avg = sum(base['sched_time']) / len(base['sched_time'])
    plt.plot(base['duration'], base['sched_time'], color='b',
             marker='s', linestyle='-.', label=''.join(['base avg:', str(round(base_avg, 2))]))

    current = pandas.read_csv(current_data, delim_whitespace=True)
    current_avg = sum(current['sched_time']) / len(current['sched_time'])
    plt.plot(current['duration'], current['sched_time'],
             color='r', marker='o', linestyle='-', label=''.join(['current avg:', str(round(current_avg, 2))]))

    ratio = (current_avg - base_avg) / base_avg

    plt.legend(loc='best')
    plt.xlabel('task_duration(us)')
    plt.ylabel('sched_time(us)')
    plt.title(''.join(['ffrt serial sched time vs base [Ratio:', str(
        round(ratio * 100, 2)), '%]']), fontsize=15)
    plt.savefig("serial_sched_time_vs_base.jpg")


if __name__ == '__main__':
    plot_current_vs_base(sys.argv)
    plot_current_olny(sys.argv)
