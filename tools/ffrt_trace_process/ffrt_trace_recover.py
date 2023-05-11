#!/usr/bin/env python3
# -*- coding: UTF-8 -*-

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
import re
import argparse
import stat
from ffrt_trace_process import extract_process_id


def extract_timestamp_str(log):
    """
    extract timestamp(us) from trace line
    """
    m = re.search(r" (\d+)\.(\d+): ", log)
    if m is None:
        return 0

    match = m.group()

    return match.strip()[:-1]


def extract_cpu_id_str(log):
    """
    extract #cpu from trace line
    """
    m = re.search(r"\) \[.*\]", log)
    if m is None:
        return -1

    match = m.group()

    return match.split(']')[0].split('[')[-1]


def make_costart_fake_log(mark, pid, label, gid, tid, tname, prio):
    """
    when ffrt task start running, make a fake log that sched_switch from ffrt thread -> ffrt task
    """
    timestamp = extract_timestamp_str(mark)
    cpu_id = extract_cpu_id_str(mark)
    fake_log = mark + "\n  %s-%d    (%7d) [%s] ....   %s: sched_switch: prev_comm=%s prev_pid=%d prev_prio=%d prev_state=S ==> next_comm=%s next_pid=10000%d next_prio=%d\n" % (
        tname, tid, pid, cpu_id, timestamp, tname, tid, prio, label, gid, prio)

    return fake_log


def make_coyield_fake_log(mark, pid, label, gid, tid, tname, prio):
    """
    when ffrt task leave running, make a fake log that sched_switch from ffrt task -> ffrt thread
    """
    timestamp = extract_timestamp_str(mark)
    cpu_id = extract_cpu_id_str(mark)
    fake_log = "  %s-10000%d    (%7d) [%s] ....   %s: sched_switch: prev_comm=%s prev_pid=10000%d prev_prio=%d prev_state=S ==> next_comm=%s next_pid=%d next_prio=%d\n" % (
        label, gid, pid, cpu_id, timestamp, label, gid, prio, tname, tid, prio)

    return fake_log


def replace_sched_switch_log(fake_log, mark, pid, label, gid, tid):
    """
    replace ffrt worker sched_swtich log with ffrt task
    """
    if "prev_pid=%d" % tid in mark:
        index = re.search("\(.+\)\s+\[\d", fake_log).span()[0]
        fake_log = "  %s-10000%d " % (label, gid) + fake_log[index:]
        fake_log = fake_log[:fake_log.index("prev_comm=")] + "prev_comm=%s " % label + \
                   fake_log[fake_log.index("prev_pid="):]
        fake_log = fake_log[:fake_log.index("prev_pid=")] + "prev_pid=10000%d " % gid + \
                   fake_log[fake_log.index("prev_prio="):]
    elif "next_pid=%d" % tid in mark:
        fake_log = fake_log[:fake_log.index("next_comm=")] + "next_comm=%s " % label + \
                   fake_log[fake_log.index("next_pid="):]
        fake_log = fake_log[:fake_log.index("next_pid=")] + "next_pid=10000%d " % gid + \
                   fake_log[fake_log.index("next_prio="):]

    return fake_log


def replace_sched_wake_log(fake_log, label, gid):
    """
    replace ffrt worker sched_wake log with ffrt task
    """
    fake_log = fake_log[:fake_log.index("comm=")] + "comm=%s " % label + fake_log[fake_log.index("pid="):]
    fake_log = fake_log[:fake_log.index("pid=")] + "pid=10000%d " % gid + fake_log[fake_log.index("prio="):]

    return fake_log


def replace_sched_block_log(fake_log, gid):
    """
    replace ffrt worker sched_block log with ffrt task
    """
    fake_log = fake_log[:fake_log.index("pid=")] + "pid=10000%d " % gid + fake_log[fake_log.index("iowait="):]

    return fake_log


def replace_tracing_mark_log(fake_log, label, gid):
    """
    replace ffrt worker normal tracing log with ffrt task
    """
    index = re.search("\(.+\)\s+\[\d", fake_log).span()[0]
    fake_log = "  %s-10000%d " % (label, gid) + fake_log[index:]

    return fake_log


def convert_worker_log_to_task(mark, pid, label, gid, tid):
    """
    convert ffrt worker trace logs to ffrt task trace logs
    """
    fake_log = mark

    if "sched_switch: " in mark:
        return replace_sched_switch_log(fake_log, mark, pid, label, gid, tid)

    if ": sched_wak" in mark:
        return replace_sched_wake_log(fake_log, label, gid)

    if "sched_blocked_reason: " in mark:
        return replace_sched_block_log(fake_log, gid)

    return replace_tracing_mark_log(fake_log, label, gid)


def find_ffrt_process_and_classify_logs(log, lineno, trace_map, ffrt_pids):
    """
    find ffrt related process and threads (ffrtwk/ffrt_io), and classify logs for threads
    """
    if "prev_comm=ffrt" in log:
        pid = extract_process_id(log)
        if pid not in ffrt_pids.keys():
            ffrt_pids[pid] = {}
        tname = log[log.index("prev_comm="):].split("prev_pid=")[0].split('=')[-1].rstrip()
        tid = int(log[log.index("prev_pid="):].split(' ')[0].split('=')[-1])
        if tid not in ffrt_pids[pid].keys():
            ffrt_pids[pid][tid] = {"name": tname, "logs": []}

    if "sched_switch: " in log:
        prev_tid = int(log[log.index("prev_pid="):].split(' ')[0].split('=')[-1])
        next_tid = int(log[log.index("next_pid="):].split(' ')[0].split('=')[-1])
        if prev_tid not in trace_map.keys():
            trace_map[prev_tid] = []
        trace_map[prev_tid].append(lineno)
        if next_tid not in trace_map.keys():
            trace_map[next_tid] = []
        trace_map[next_tid].append(lineno)
        return

    if ": sched_wak" in log or "sched_blocked_reason: " in log:
        tid = int(log[log.index("pid="):].split(' ')[0].split('=')[-1])
        if tid not in trace_map.keys():
            trace_map[tid] = []
        trace_map[tid].append(lineno)
        return

    match = re.search("\(.+\)\s+\[\d", log)
    if match is not None:
        tid = int(log[:match.span()[0]].split('-')[-1])
        if tid not in trace_map.keys():
            trace_map[tid] = []
        trace_map[tid].append(lineno)

    return


def classify_logs_for_ffrt_worker(logs):
    """
    split traces that written in the same line and classify logs based on ffrt worker
    """
    trace_map = {}
    ffrt_pids = {}

    lineno = 0
    while lineno < len(logs):
        log = logs[lineno]

        indexs = [m.span()[0] for m in re.finditer("\S+-\d+\s+\(", log)]

        if len(indexs) > 1:
            del logs[lineno]
            for j in range(len(indexs)):
                begin = indexs[j]
                end = indexs[j + 1] if j + 1 < len(indexs) else len(log)
                log_split = log[begin:end]
                if j + 1 < len(indexs):
                    log_split = "%s\n" % log_split
                logs.insert(lineno + j, log_split)

                find_ffrt_process_and_classify_logs(logs[lineno + j], lineno + j, trace_map, ffrt_pids)

            lineno += len(indexs) - 1
        else:
            find_ffrt_process_and_classify_logs(logs[lineno], lineno, trace_map, ffrt_pids)

        lineno += 1

    for pid, tids in ffrt_pids.items():
        for tid in tids.keys():
            ffrt_pids[pid][tid]["logs"] = trace_map[tid]

    return ffrt_pids


def convert_ffrt_thread_to_ffrt_task(logs, ffrt_pids):
    """
    convert tracing mark of ffrt worker to ffrt task
    """
    task_labels = {}

    for pid, tids in ffrt_pids.items():
        task_labels[pid] = {}
        for tid, info in ffrt_pids[pid].items():
            tname = info["name"]
            linenos = info["logs"]
            prio = 120

            task_running = None
            for lineno in linenos:
                mark = logs[lineno]

                if "sched_switch: " in mark:
                    if "prev_pid=%d" % tid in mark:
                        prio = int(mark[mark.index("prev_prio="):].split(' ')[0].split('=')[-1])
                    elif "next_pid=%d" % tid in mark:
                        prio = int(mark[mark.index("next_prio="):].split(' ')[0].split('=')[-1])

                # FFRT Task Running
                if "FFRT::[" in mark:
                    label = mark.split('[')[-1].split(']')[0]
                    gid = int(mark.split('|')[-1])
                    if gid not in task_labels[pid].keys():
                        task_labels[pid][gid] = label

                    task_running = gid
                    fake_log = make_costart_fake_log(mark, pid, task_labels[pid][task_running], task_running,
                                                     tid, tname, prio)
                    logs[lineno] = fake_log
                    continue

                if task_running is not None:
                    # FFRT Task Blocked/Finished Marks
                    if re.search(r" F\|(\d+)\|[BF]\|(\d+)", mark) is not None:
                        fake_log = make_coyield_fake_log(mark, pid, task_labels[pid][task_running], task_running,
                                                         tid, tname, prio)
                        logs[lineno] = fake_log
                        task_running = None
                        continue

                    fake_log = convert_worker_log_to_task(mark, pid, task_labels[pid][task_running], task_running,
                                                          tid)
                    logs[lineno] = fake_log
                    continue

    return


def main():
    parser = argparse.ArgumentParser(description="parse")
    parser.add_argument('--file', '-f', type=str, required=True, help="input trace file path")

    args = parser.parse_args()

    if not os.path.isfile(args.file):
        exit(1)

    with open(args.file, 'r') as infile:
        logs = infile.readlines()

        ffrt_pids = classify_logs_for_ffrt_worker(logs)

        convert_ffrt_thread_to_ffrt_task(logs, ffrt_pids)

        file_name, file_ext = os.path.splitext(args.file)[0], os.path.splitext(args.file)[1]

        with os.fdopen(os.open("%s_ffrt_recover%s" % (file_name, file_ext), os.O_WRONLY | os.O_CREAT,
                    stat.S_IWUSR | stat.S_IRUSR), 'w') as outfile:
            outfile.writelines(logs)
            outfile.close()

        infile.close()

    return


if __name__ == "__main__":
    main()