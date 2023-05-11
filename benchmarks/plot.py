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
import pandas
from matplotlib import pyplot

stall = [
    0, 5, 10, 15, 20, 25, 30, 35, 40, 50,
    60, 70, 80, 90, 100, 120, 140, 160, 180, 200
]


def draw_fig(ax, base, data, title):
    label = title[:-4]
    thread = title[-3:]

    # get optimization ratio
    avg = sum(data) / len(data)
    avg_base = base['_'.join([label, 'base', thread])].mean()
    ratio = str(round((avg_base - avg) / avg_base, 3))

    data_base = [float(base['_'.join([label, 'base', thread])][i])
                 for i in range(len(stall))]
    ax.plot(stall, data_base, 's-', label='_'.join([label, 'base']))
    ax.plot(stall, data, 'o-', label=label)
    ax.legend(loc='best')
    ax.set_title(''.join([title, '[R:', ratio, ']']))


def main(argv):
    path = argv[1]
    out = argv[2]

    # read csv
    base = pandas.read_csv(os.path.join(path, 'base.csv'))
    t1 = pandas.read_csv(os.path.join(path, 'perf_thread1.csv'))
    t8 = pandas.read_csv(os.path.join(path, 'perf_thread8.csv'))

    # get actual data
    t1_fork_join = [(t1[' fork_join'][i] / t1['repeat'][i] *
                     1 - stall[i] * 10000) / 10000 for i in range(len(stall))]
    t1_fork_join_worker_submit = [(t1['fork_join_worker_submit'][i] / t1['repeat']
                                   [i] * 1 - stall[i] * 10000) / 10000 for i in range(len(stall))]
    t1_airaw = [(t1['airaw'][i] / t1['repeat'][i] * 1 - stall[i]
                 * 2400 * 3) / 2400 / 3 for i in range(len(stall))]
    t1_airaw_worker_submit = [(t1['airaw_worker_submit'][i] / t1['repeat']
                               [i] * 1 - stall[i] * 2400 * 3) / 2400 / 3 for i in range(len(stall))]
    t1_fib_data_wait = [(t1['fib_data_wait'][i] / t1['repeat'][i]
                         * 1 - stall[i] * 21890) / 21890 for i in range(len(stall))]
    t1_fib_child_wait = [(t1['fib_child_wait'][i] / t1['repeat'][i]
                          * 1 - stall[i] * 21890) / 21890 for i in range(len(stall))]
    t1_fib_no_wait = [(t1['fib_no_wait'][i] / t1['repeat'][i]
                       * 1 - stall[i] * 21890) / 21890 for i in range(len(stall))]
    t8_fork_join = [(t8[' fork_join'][i] / t8['repeat'][i] *
                     8 - stall[i] * 10000) / 10000 for i in range(len(stall))]
    t8_fork_join_worker_submit = [(t8['fork_join_worker_submit'][i] / t8['repeat']
                                   [i] * 8 - stall[i] * 10000) / 10000 for i in range(len(stall))]
    t8_airaw = [(t8['airaw'][i] / t8['repeat'][i] * 3 - stall[i]
                 * 2400 * 3) / 2400 / 3 for i in range(len(stall))]
    t8_airaw_worker_submit = [(t8['airaw_worker_submit'][i] / t8['repeat']
                               [i] * 3 - stall[i] * 2400 * 3) / 2400 / 3 for i in range(len(stall))]
    t8_fib_data_wait = [(t8['fib_data_wait'][i] / t8['repeat'][i]
                         * 8 - stall[i] * 21890) / 21890 for i in range(len(stall))]
    t8_fib_child_wait = [(t8['fib_child_wait'][i] / t8['repeat'][i]
                          * 8 - stall[i] * 21890) / 21890 for i in range(len(stall))]
    t8_fib_no_wait = [(t8['fib_no_wait'][i] / t8['repeat'][i]
                       * 8 - stall[i] * 21890) / 21890 for i in range(len(stall))]

    # draw figure
    fig, ax = pyplot.subplots(2, 7, figsize=(30, 8))

    draw_fig(ax[0, 0], base, t1_fork_join, 'fork_join_1th')
    draw_fig(ax[0, 1], base, t1_fork_join_worker_submit,
             'fork_join_worker_submit_1th')
    draw_fig(ax[0, 2], base, t1_airaw, 'airaw_1th')
    draw_fig(ax[0, 3], base, t1_airaw_worker_submit, 'airaw_worker_submit_1th')
    draw_fig(ax[0, 4], base, t1_fib_data_wait, 'fib20_data_wait_1th')
    draw_fig(ax[0, 5], base, t1_fib_child_wait, 'fib20_child_wait_1th')
    draw_fig(ax[0, 6], base, t1_fib_no_wait, 'fib20_no_wait_1th')
    draw_fig(ax[1, 0], base, t8_fork_join, 'fork_join_8th')
    draw_fig(ax[1, 1], base, t8_fork_join_worker_submit,
             'fork_join_worker_submit_8th')
    draw_fig(ax[1, 2], base, t8_airaw, 'airaw_8th')
    draw_fig(ax[1, 3], base, t8_airaw_worker_submit, 'airaw_worker_submit_8th')
    draw_fig(ax[1, 4], base, t8_fib_data_wait, 'fib20_data_wait_8th')
    draw_fig(ax[1, 5], base, t8_fib_child_wait, 'fib20_child_wait_8th')
    draw_fig(ax[1, 6], base, t8_fib_no_wait, 'fib20_no_wait_8th')

    fig.savefig(os.path.join(path, out))


if __name__ == '__main__':
    main(sys.argv)
