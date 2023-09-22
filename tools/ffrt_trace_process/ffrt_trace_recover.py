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
import copy
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
    fake_log = mark + "\n  %s-%d    (%7d) [%s] ....   %s: sched_switch: prev_comm=%s prev_pid=%d prev_prio=%d prev_state=S ==> next_comm=%s next_pid=%d0%d next_prio=%d\n" % (
        tname, tid, pid, cpu_id, timestamp, tname, tid, prio, label, pid, gid, prio)

    return fake_log


def make_coyield_fake_log(mark, pid, label, gid, tid, tname, prio):
    """
    when ffrt task leave running, make a fake log that sched_switch from ffrt task -> ffrt thread
    """
    timestamp = extract_timestamp_str(mark)
    cpu_id = extract_cpu_id_str(mark)
    fake_log = "  %s-%d0%d    (%7d) [%s] ....   %s: sched_switch: prev_comm=%s prev_pid=%d0%d prev_prio=%d prev_state=S ==> next_comm=%s next_pid=%d next_prio=%d\n" % (
        label, pid, gid, pid, cpu_id, timestamp, label, pid, gid, prio, tname, tid, prio)

    if "|B|" in mark or "|H:B " in mark:
        fake_log = "  %s-%d0%d    (%7d) [%s] ....   %s: tracing_mark_write: E|%d\n" % \
                   (label, pid, gid, pid, cpu_id, timestamp, pid) + fake_log

    return fake_log


def replace_sched_switch_log(fake_log, mark, pid, label, gid, tid):
    """
    replace ffrt worker sched_swtich log with ffrt task
    """
    if "prev_pid=%d" % tid in mark:
        index = re.search("\(.+\)\s+\[\d", fake_log).span()[0]
        fake_log = "  %s-%d0%d " % (label, pid, gid) + fake_log[index:]
        fake_log = fake_log[:fake_log.index("prev_comm=")] + "prev_comm=%s " % label + \
                   fake_log[fake_log.index("prev_pid="):]
        fake_log = fake_log[:fake_log.index("prev_pid=")] + "prev_pid=%d0%d " % (pid, gid) + \
                   fake_log[fake_log.index("prev_prio="):]
    elif "next_pid=%d" % tid in mark:
        fake_log = fake_log[:fake_log.index("next_comm=")] + "next_comm=%s " % label + \
                   fake_log[fake_log.index("next_pid="):]
        fake_log = fake_log[:fake_log.index("next_pid=")] + "next_pid=%d0%d " % (pid, gid) + \
                   fake_log[fake_log.index("next_prio="):]

    return fake_log


def replace_sched_wake_log(fake_log, label, pid, gid):
    """
    replace ffrt worker sched_wake log with ffrt task
    """
    fake_log = fake_log[:fake_log.index("comm=")] + "comm=%s " % label + fake_log[fake_log.index("pid="):]
    fake_log = fake_log[:fake_log.index("pid=")] + "pid=%d0%d " % (pid, gid) + fake_log[fake_log.index("prio="):]

    return fake_log


def replace_sched_block_log(fake_log, pid, gid):
    """
    replace ffrt worker sched_block log with ffrt task
    """
    fake_log = fake_log[:fake_log.index("pid=")] + "pid=%d0%d " % (pid, gid) + fake_log[fake_log.index("iowait="):]

    return fake_log


def replace_tracing_mark_log(fake_log, label, pid, gid):
    """
    replace ffrt worker normal tracing log with ffrt task
    """
    index = re.search("\(.+\)\s+\[\d", fake_log).span()[0]
    fake_log = "  %s-%d0%d " % (label, pid, gid) + fake_log[index:]

    return fake_log


def convert_worker_log_to_task(mark, pid, label, gid, tid):
    """
    convert ffrt worker trace logs to ffrt task trace logs
    """
    fake_log = mark

    if "sched_switch: " in mark:
        return replace_sched_switch_log(fake_log, mark, pid, label, gid, tid)

    if ": sched_wak" in mark:
        return replace_sched_wake_log(fake_log, label, pid, gid)

    if "sched_blocked_reason: " in mark:
        return replace_sched_block_log(fake_log, pid, gid)

    return replace_tracing_mark_log(fake_log, label, pid, gid)


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

    match = re.search(" \(.+\)\s+\[\d", log)
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
                if "<faulted>" in logs[lineno + j]:
                    pid = extract_process_id(logs[lineno + j])
                    logs[lineno + j] = logs[lineno + j][:logs[lineno + j].index("<faulted>")] + "E|%d\n" % pid

            lineno += len(indexs) - 1
        else:
            find_ffrt_process_and_classify_logs(logs[lineno], lineno, trace_map, ffrt_pids)
            if "<faulted>" in logs[lineno]:
                pid = extract_process_id(logs[lineno])
                logs[lineno] = logs[lineno][:logs[lineno].index("<faulted>")] + "E|%d\n" % pid

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

            switch_in_fake_log = False
            switch_out_fake_log = False
            ffbk_mark_remove = False
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
                    miss_log = None
                    if task_running is not None:  # trace end missing
                        miss_log = make_coyield_fake_log(mark, pid, task_labels[pid][task_running], task_running,
                                                         tid, tname, prio)
                        timestamp = extract_timestamp_str(mark)
                        cpu_id = extract_cpu_id_str(mark)
                        miss_log += "  %s-%d    (%7d) [%s] ....   %s: tracing_mark_write: E|%d\n" % (
                                        tname, tid, pid, cpu_id, timestamp, pid)

                    label = mark.split('[')[-1].split(']')[0]
                    try:
                        gid = int(mark.split('|')[-1])
                    except ValueError:
                        continue
                    if gid not in task_labels[pid].keys():
                        task_labels[pid][gid] = label

                    task_running = gid
                    fake_log = make_costart_fake_log(mark, pid, task_labels[pid][task_running], task_running,
                                                     tid, tname, prio)
                    logs[lineno] = fake_log

                    if miss_log is not None:
                        logs[lineno] = miss_log + logs[lineno]

                    switch_in_fake_log = True
                    continue

                if task_running is not None:
                    # Remove FFRT Supplemented Log for CoSwitch
                    if re.search(r" F\|(\d+)\|Co\|(\d+)", mark) is not None or \
                            re.search(r" F\|(\d+)\|H:Co\s(\d+)", mark) is not None:
                        logs[lineno] = "\n"
                        if switch_in_fake_log is True:
                            switch_in_fake_log = False
                            continue
                        else:
                            switch_out_fake_log = True
                            continue
                    if switch_in_fake_log is True and "tracing_mark_write: B" in mark:
                        logs[lineno] = "\n"
                        continue
                    if switch_out_fake_log is True and "tracing_mark_write: E" in mark:
                        logs[lineno] = "\n"
                        continue

                    # FFRT Task Blocked/Finished Marks
                    if re.search(r" F\|(\d+)\|[BF]\|(\d+)", mark) is not None or \
                            re.search(r" F\|(\d+)\|H:[BF]\s(\d+)", mark) is not None:
                        fake_log = make_coyield_fake_log(mark, pid, task_labels[pid][task_running], task_running,
                                                         tid, tname, prio)
                        logs[lineno] = fake_log
                        task_running = None

                        if switch_out_fake_log is True:
                            switch_out_fake_log = False

                        continue

                    if ffbk_mark_remove is True and "tracing_mark_write: E" in mark:
                        logs[lineno] = "\n"
                        ffbk_mark_remove = False
                        continue

                    fake_log = convert_worker_log_to_task(mark, pid, task_labels[pid][task_running], task_running,
                                                          tid)

                    # FFRT Blocked Reason
                    if "FFBK[" in fake_log:
                        if "[dep]" in fake_log:
                            fake_log = "\n"
                        elif "[chd]" in fake_log:
                            fake_log = "%sFFBK[wait_child]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 9:])
                        elif "[dat]" in fake_log:
                            fake_log = "%sFFBK[wait_data]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 9:])
                        elif "[fd]" in fake_log:
                            fake_log = "%sFFBK[wait_fd]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 8:])
                        elif "[mtx]" in fake_log:
                            fake_log = "%sFFBK[mutex]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 9:])
                        elif "[slp]" in fake_log:
                            fake_log = "%sFFBK[sleep]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 9:])
                        elif "[yld]" in fake_log:
                            fake_log = "%sFFBK[yield]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 9:])
                        elif "[cnd]" in fake_log:
                            fake_log = "%sFFBK[cond_wait]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 9:])
                        elif "[cnt]" in fake_log:
                            fake_log = "%sFFBK[cond_timedwait]%s" % (
                                fake_log[:fake_log.index("FFBK[")], fake_log[fake_log.index("FFBK[") + 9:])

                        ffbk_mark_remove = True

                    logs[lineno] = fake_log
                    continue

    return


def supplement_ffrt_block_and_wake_info(logs):
    """
    supplement ffrt block slice and link ffrt wake with ffrt block
    """
    task_labels = {}

    for lineno in range(len(logs)):
        log = logs[lineno]

        if "FFBK[" in log:
            pid = extract_process_id(log)
            gid = int(log.split('|')[-1])

            if pid not in task_labels.keys():
                task_labels[pid] = {}
            if gid not in task_labels[pid].keys():
                task_labels[pid][gid] = {
                    "state": "none",
                    "prev_wake_lineno": None,
                    "prev_wake_log": None,
                }

            ready_end_log = None
            if task_labels[pid][gid]["state"] == "ready":  # abnormal scene: ready -> block, running missed
                timestamp = extract_timestamp_str(log)
                cpu_id = extract_cpu_id_str(log)
                ready_end_log = "  <...>-%d0%d    (%7d) [%s] ....   %s: tracing_mark_write: E|%d\n" % (
                    pid, gid, pid, cpu_id, timestamp, pid)

            task_labels[pid][gid]["state"] = "block"

            logs[lineno] = "%s\n" % logs[lineno][:logs[lineno].rfind('|')]

            if ready_end_log is not None:
                logs[lineno] = ready_end_log + logs[lineno]

            continue

        if "FFWK|" in log:
            pid = extract_process_id(log)
            gid = int(log.split('|')[-1])

            if pid in task_labels.keys() and gid in task_labels[pid].keys():
                timestamp = extract_timestamp_str(log)
                cpu_id = extract_cpu_id_str(log)
                if "H:FFWK" in log:
                    ready_begin_log = "  <...>-%d0%d    (%7d) [%s] ....   %s: tracing_mark_write: B|%d|H:FFREADY\n" % (
                        pid, gid, pid, cpu_id, timestamp, pid)
                else:
                    ready_begin_log = "  <...>-%d0%d    (%7d) [%s] ....   %s: tracing_mark_write: B|%d|FFREADY\n" % (
                        pid, gid, pid, cpu_id, timestamp, pid)
                logs[lineno] = ready_begin_log + logs[lineno]

                if task_labels[pid][gid]["state"] == "ready":  # abnormal scene: ready -> ready, running & block missed
                    logs[task_labels[pid][gid]["prev_wake_lineno"]] = logs[task_labels[pid][gid]["prev_wake_lineno"]][
                                                                      logs[task_labels[pid][gid][
                                                                          "prev_wake_lineno"]].index("FFREADY") + 8:]

                task_labels[pid][gid]["state"] = "ready"
                task_labels[pid][gid]["prev_wake_log"] = copy.copy(log)
                task_labels[pid][gid]["prev_wake_lineno"] = lineno

            continue

        if "FFRT::[" in log:
            pid = extract_process_id(log)
            try:
                gid = int([e for e in log.split('\n') if "FFRT::[" in e][0].split('|')[-1])
            except ValueError:
                continue

            if pid in task_labels.keys() and gid in task_labels[pid].keys():
                if task_labels[pid][gid]["state"] == "ready":
                    timestamp = extract_timestamp_str(log)
                    cpu_id = extract_cpu_id_str(log)

                    switch_log = log.split('\n')[-2]
                    task_comm = switch_log[switch_log.index("next_comm="):].split("next_pid=")[0].split('=')[
                        -1].rstrip()
                    task_pid = int(switch_log[switch_log.index("next_pid="):].split(' ')[0].split('=')[-1])
                    task_prio = int(switch_log[switch_log.index("next_prio="):].split('=')[-1])
                    cpu_id_wake = extract_cpu_id_str(switch_log)
                    waking_log = task_labels[pid][gid]["prev_wake_log"][:task_labels[pid][gid]["prev_wake_log"].index(
                        "tracing_mark_write:")] + "sched_waking: comm=%s pid=%d prio=%d target_cpu=%s\n" % (
                                 task_comm, task_pid, task_prio, cpu_id_wake)
                    wakeup_log = task_labels[pid][gid]["prev_wake_log"][
                                 :task_labels[pid][gid]["prev_wake_log"].index("tracing_mark_write:")] + \
                                 "sched_wakeup: comm=%s pid=%d prio=%d target_cpu=%s\n" % (
                                 task_comm, task_pid, task_prio, cpu_id_wake)
                    logs[task_labels[pid][gid]["prev_wake_lineno"]] += waking_log + wakeup_log

                    ready_end_log = "  <...>-%d0%d    (%7d) [%s] ....   %s: tracing_mark_write: E|%d\n" % (
                        pid, gid, pid, cpu_id, timestamp, pid)
                    logs[lineno] = ready_end_log + logs[lineno]

                    task_labels[pid][gid]["state"] = "none"

            continue

    return


def trace_recover(file):
    with open(file, 'r', encoding="gb18030", errors="ignore") as infile:
        logs = infile.readlines()

        ffrt_pids = classify_logs_for_ffrt_worker(logs)

        convert_ffrt_thread_to_ffrt_task(logs, ffrt_pids)

        supplement_ffrt_block_and_wake_info(logs)

        file_name, file_ext = os.path.splitext(file)[0], os.path.splitext(file)[1]

        with os.fdopen(os.open("%s_ffrt_recover%s" % (file_name, file_ext), os.O_WRONLY | os.O_CREAT | os.O_TRUNC,
                    stat.S_IWUSR | stat.S_IRUSR), 'w', encoding="gb18030", errors="ignore") as outfile:
            outfile.writelines(logs)
            outfile.close()

        infile.close()

    return


def main():
    parser = argparse.ArgumentParser(description="parse")
    parser.add_argument('--file', '-f', type=str, required=True, help="input trace file path")

    args = parser.parse_args()

    if not os.path.isfile(args.file):
        exit(1)

    trace_recover(args.file)

    return


if __name__ == "__main__":
    main()