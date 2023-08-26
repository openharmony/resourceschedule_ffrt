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

import copy
import os
import stat
import re
import argparse
import csv
import logging

# global variables
WORKER_KEY = ["ffrt_worker-", "ffrtwk", "ffrt_rtg-"]


def extract_thread_name(log):
    """
    extract thread name from trace line
    """
    return log.strip().split(' ')[0]


def extract_thread_id(log):
    """
    extract tid from trace line
    """
    if len(log.strip().split(' ')[0].split('-')) <= 1:
        return 0

    return int(log.strip().split(' ')[0].split('-')[-1])


def extract_process_id(log):
    """
    extract pid from trace line
    """
    m = re.search(r"\(\s*\d+\) \[", log)
    if m is None:
        return 0

    match = m.group()
    if '-' in match:
        return 0

    return int(match.split(')')[0].lstrip('('))


def extract_cpu_id(log):
    """
    extract #cpu from trace line
    """
    m = re.search(r"\) \[.*\]", log)
    if m is None:
        return -1

    match = m.group()

    return int(match.split(']')[0].split('[')[-1])


def extract_timestamp(log):
    """
    extract timestamp(us) from trace line
    """
    m = re.search(r" (\d+)\.(\d+): ", log)
    if m is None:
        return 0

    match = m.group()

    return int(match.strip().split('.')[0]) * int(1e6) + int(match.strip().rstrip(':').split('.')[-1])


def extract_trace_marker_task(log):
    """
    extract ffrt task marker from trace line
    """
    m = re.search(r" [FB]\|(\d+)\|(.+)\|(\d+)", log)

    if m is not None:
        match = m.group()

        return (match.split('|')[-2], int(match.split('|')[-1]))

    m = re.search(r" F\|(\d+)\|(\S+)\s(\d+)", log)

    if m is not None:
        match = m.group()

        return (match.split('|')[-1].split(' ')[0], int(match.split(' ')[-1]))

    return (False, False)


def extract_switch_info(log):
    """
    parse sched_switch log
    """
    switch_info = {}

    switch_info["cpu"] = extract_cpu_id(log)
    switch_info["timestamp"] = extract_timestamp(log)

    index = log.index("prev_comm=")
    switch_info["prev_tname"] = log[index:].split("prev_pid=")[0].split('=')[-1].rstrip()

    index = log.index("prev_pid=")
    switch_info["prev_tid"] = int(log[index:].split(' ')[0].split('=')[-1])

    index = log.index("prev_state=")
    switch_info["prev_state"] = log[index:].split(' ')[0].split('=')[-1]

    index = log.index("next_comm=")
    switch_info["next_tname"] = log[index:].split("next_pid=")[0].split('=')[-1].rstrip()

    index = log.index("next_pid=")
    switch_info["next_tid"] = int(log[index:].split(' ')[0].split('=')[-1])

    return switch_info


def extract_active_pid_and_switch_log(logs):
    """
    extract active processes in trace with corresponding switch logs
    """
    active_process_map = {}
    switch_log_map = {}
    ffrt_process = []

    for log in logs:
        if " sched_" in log or " tracing_mark_write" in log:
            pid = extract_process_id(log)
            if pid != 0 and pid not in active_process_map.keys():
                active_process_map[pid] = {}

            tn = log[:log.find(" (")].strip()
            ti = int(tn.split('-')[-1])
            if ti != 0 and ti not in active_process_map[pid].keys():
                active_process_map[pid][ti] = tn

            if "sched_switch:" in log:
                switch_info = extract_switch_info(log)
                if switch_info["prev_tid"] not in switch_log_map.keys():
                    switch_log_map[switch_info["prev_tid"]] = []
                switch_log_map[switch_info["prev_tid"]].append(switch_info)

                if switch_info["next_tid"] not in switch_log_map.keys():
                    switch_log_map[switch_info["next_tid"]] = []
                switch_log_map[switch_info["next_tid"]].append(switch_info)

                if "ffrt" in switch_info["prev_tname"] and pid not in ffrt_process:
                    ffrt_process.append(pid)

                if pid != 0 and switch_info["prev_tname"] not in active_process_map[pid][ti]:
                    active_process_map[pid][ti] = \
                        "%s-%d" % (switch_info["prev_tname"], switch_info["prev_tid"])

    return ffrt_process, active_process_map, switch_log_map


def parse_thread_trace(switch_logs, tid):
    """
    parser trace record of specific thread：
        1）sched_waking：waking up thread
        2）sched_blocked_reason：uninterruptible sleep
        3）sched_wakeup：thread waked up
        4）sched_switch：thread switch out/in
    note that trace file may lose some logs during recording
    therefore approximate esimation is used in statistics
    """
    statistics = {
        "running": {
            "duration": 0, "occurrence": 0, "average": 0.
        },
        "cpu": {},
        "switch_out": {}
    }

    prev_timestamp = None
    prev_running = None

    for switch_log in switch_logs:
        if switch_log["next_tid"] == tid:
            statistics["running"]["occurrence"] += 1
            if prev_running == "running":
                continue
            prev_timestamp = switch_log["timestamp"]
            prev_running = "running"
        elif switch_log["prev_tid"] == tid:
            curr_timestamp = switch_log["timestamp"]
            if prev_running == "running":
                statistics["running"]["duration"] += curr_timestamp - prev_timestamp
                if switch_log["cpu"] not in statistics["cpu"].keys():
                    statistics["cpu"][switch_log["cpu"]] = 0
                statistics["cpu"][switch_log["cpu"]] += curr_timestamp - prev_timestamp
            prev_timestamp = curr_timestamp
            prev_running = "idle"
            if switch_log["prev_state"] not in statistics["switch_out"].keys():
                statistics["switch_out"][switch_log["prev_state"]] = 0
            statistics["switch_out"][switch_log["prev_state"]] += 1

    statistics["running"]["average"] = float(statistics["running"]["duration"]) / float(
        statistics["running"]["occurrence"]) if statistics["running"]["occurrence"] != 0 else 0.

    return statistics


def generate_counter_info(suffix, task_records, next_status, gid, pid_counters_dict, pid):
    """
    generate trace counter marker based on task status transition
    """
    infos = []
    prev_counter = task_records[gid]["status"] + "_task"
    next_counter = next_status + "_task"

    if next_status != "finish":
        pid_counters_dict[next_counter] += 1
        info = suffix + "C|" + str(pid) + "|" + next_counter + "|" + str(pid_counters_dict[next_counter]) + '\n'
        infos.append(info)

    if next_status != "pending":
        pid_counters_dict[prev_counter] -= 1
        info = suffix + "C|" + str(pid) + "|" + prev_counter + "|" + str(pid_counters_dict[prev_counter]) + '\n'
        infos.append(info)

    task_records[gid]["status"] = next_status

    return infos


def parse_and_convert_task_trace(logs, pid):
    """
    parser trace record of ffrt tasks：
        P：task submit
        R：task ready
        E：task execute
        B：task block
        F：task execute finished
    convert task execute record from async marker to sync marker
    """
    task_records = {}
    task_infos = {}
    submit_no_ready_tasks = {}
    ready_no_exec_tasks = {}
    exec_no_done_tasks = {}
    pid_counters_dict = {
        'total_task': 0,
        'pending_task': 0,
        'ready_task': 0,
        'running_task': 0,
        'blocked_task': 0
    }

    pid_keyword = "%d) " % pid
    trace_end_keyword = " E|%d" % pid

    lineno = 0
    remove_trace_end = False
    logs_supplement = []
    for log in logs:
        lineno += 1

        if pid_keyword not in log or "tracing_mark_write: " not in log:
            logs_supplement.append(log)
            continue

        if remove_trace_end is True and trace_end_keyword in log:
            remove_trace_end = False
            continue

        task_marker = extract_trace_marker_task(log)
        if len(task_marker) == 0 or task_marker[0] is False:
            logs_supplement.append(log)
            continue

        state = task_marker[0]
        if "H:" in state:
            state = state[2:]
        gid = task_marker[1]
        suffix = log[:log.find("tracing_mark_write: ") + len("tracing_mark_write: ")]

        if "P[" in state:
            if gid not in task_records.keys():
                tag = state.split('[')[-1].split(']')[0]
                task_records[gid] = {
                    "gid": gid,
                    "tag": tag,
                    "submit": extract_timestamp(log),
                    "ready": None,
                    "exec": None,
                    "cowait": [],
                    "costart": [],
                    "done": None,
                    "exec_duration": 0,
                    "cowait_duration": 0,
                    "exec_pids": [],
                    "prev_tname": None,
                    "status": "pending",
                }

            # replace async trace begin with trace counter
            pid_counters_dict['total_task'] += 1
            line_total_task = "%sC|%d|total_task|%s\n" % (suffix, pid, str(pid_counters_dict['total_task']))
            logs_supplement.append(line_total_task)

            infos = generate_counter_info(suffix, task_records, "pending", gid, pid_counters_dict, pid)
            for info in infos:
                logs_supplement.append(info)

            remove_trace_end = True

            continue

        if state == "R":
            if gid in task_records.keys():
                if task_records[gid]["ready"] is None:
                    task_records[gid]["ready"] = extract_timestamp(log)

                # replace async trace begin with trace counter
                infos = generate_counter_info(suffix, task_records, "ready", gid, pid_counters_dict, pid)
                for info in infos:
                    logs_supplement.append(info)

            continue

        if "FFRT::[" in state:
            if gid in task_records.keys():
                timestamp = extract_timestamp(log)
                tid = extract_thread_id(log)
                if task_records[gid]["exec"] is None:
                    task_records[gid]["exec"] = timestamp
                task_records[gid]["costart"].append(timestamp)
                task_records[gid]["exec_pids"].append(tid)
                task_records[gid]["prev_tname"] = extract_thread_name(log)
                if len(task_records[gid]["cowait"]) > 0:
                    task_records[gid]["cowait_duration"] += task_records[gid]["costart"][-1] - task_records[gid]["cowait"][-1]

                # replace async trace begin with trace counter
                infos = generate_counter_info(suffix, task_records, "running", gid, pid_counters_dict, pid)
                for info in infos:
                    logs_supplement.append(info)
            logs_supplement.append(log)

            continue

        if state == "B":
            if gid in task_records.keys():
                task_records[gid]["cowait"].append(extract_timestamp(log))
                if len(task_records[gid]["costart"]) > 0:
                    task_records[gid]["exec_duration"] += task_records[gid]["cowait"][-1] - task_records[gid]["costart"][-1]

                # replace async trace begin with trace counter
                infos = generate_counter_info(suffix, task_records, "blocked", gid, pid_counters_dict, pid)
                for info in infos:
                    logs_supplement.append(info)

            continue

        if state == "F":
            if gid in task_records.keys():
                timestamp = extract_timestamp(log)
                task_records[gid]["done"] = timestamp
                if len(task_records[gid]["costart"]) > 0:
                    task_records[gid]["exec_duration"] += timestamp - task_records[gid]["costart"][-1]

                # replace async trace begin with trace counter
                infos = generate_counter_info(suffix, task_records, "finish", gid, pid_counters_dict, pid)
                for info in infos:
                    logs_supplement.append(info)

            continue

        logs_supplement.append(log)

    for task in task_records.values():
        if task["tag"] not in task_infos.keys():
            task_infos[task["tag"]] = []

        # check suspect tasks, i.e. not ready, not execute, not finish
        if task["ready"] is None:
            if task["exec"] is not None or task["done"] is not None:
                task["submit_ready"] = "lost"
            else:
                task["submit_ready"] = None
                if task["tag"] not in submit_no_ready_tasks.keys():
                    submit_no_ready_tasks[task["tag"]] = []
                submit_no_ready_tasks[task["tag"]].append(task["gid"])
        else:
            task["submit_ready"] = task["ready"] - task["submit"]

        if task["exec"] is None:
            if task["ready"] is None:
                task["ready_exec"] = None
            elif task["done"] is not None:
                task["ready_exec"] = "lost"
            else:
                task["ready_exec"] = None
                if task["tag"] not in ready_no_exec_tasks.keys():
                    ready_no_exec_tasks[task["tag"]] = []
                ready_no_exec_tasks[task["tag"]].append(task["gid"])
        else:
            if task["ready"] is None:
                task["ready_exec"] = "lost"
            else:
                task["ready_exec"] = task["exec"] - task["ready"]

        if task["done"] is None:
            task["exec_done"] = None
            if task["exec"] is not None:
                if task["tag"] not in exec_no_done_tasks.keys():
                    exec_no_done_tasks[task["tag"]] = []
                exec_no_done_tasks[task["tag"]].append(task["gid"])
        else:
            if task["exec"] is None:
                task["exec_done"] = "lost"
            else:
                task["exec_done"] = task["done"] - task["exec"]

        task_infos[task["tag"]].append(task)

    return task_infos, submit_no_ready_tasks, ready_no_exec_tasks, exec_no_done_tasks, logs_supplement


def process_trace(logs, pid, active_process_map, switch_log_map):
    """
    process trace data, generate thread info and task info
    """
    tids = list(active_process_map[pid].keys())
    tnames = list(active_process_map[pid].values())

    data = {
        "total": {},
        "switch": {
            "worker": {},
            "non-worker": {}
        },
        "cpu": {
            "worker": {},
            "non-worker": {}
        },
        "thread": {
            "worker": {
                "S": {}, "T": {}
            },
            "non-worker": {
                "S": {}, "T": {}
            },
        },
        "task": {
            "infos": None,
            "not_ready_tasks": None,
            "not_exec_tasks": None,
            "not_finish_taks": None
        }
    }

    for i in range(len(tids)):
        statistics = parse_thread_trace(switch_log_map[tids[i]], tids[i])

        tname = tnames[i]
        ttype = "worker" if any([k in tname for k in WORKER_KEY]) else "non-worker"

        # save thread slices
        data["thread"][ttype]["S"][tname] = {
            "statistics": statistics,
        }

        for cpu, duration in statistics["cpu"].items():
            if cpu not in data["cpu"][ttype].keys():
                data["cpu"][ttype][cpu] = 0
            data["cpu"][ttype][cpu] += duration

            # thread running distribution
            if tname not in data["thread"][ttype]["T"].keys():
                data["thread"][ttype]["T"][tname] = 0
            data["thread"][ttype]["T"][tname] += duration

        for state, count in statistics["switch_out"].items():
            if state not in data["switch"][ttype].keys():
                data["switch"][ttype][state] = 0
            data["switch"][ttype][state] += count

    data["total"]["all_load"] = sum(data["thread"]["worker"]["T"].values()) + sum(
        data["thread"]["non-worker"]["T"].values())
    data["total"]["worker_load"] = sum(data["thread"]["worker"]["T"].values())
    data["total"]["all_switch"] = sum(data["switch"]["worker"].values()) + sum(
        data["switch"]["non-worker"].values())
    data["total"]["worker_switch"] = sum(data["switch"]["worker"].values())

    task_infos, submit_no_ready_tasks, ready_no_exec_tasks, exec_no_done_tasks, logs_supplement = \
        parse_and_convert_task_trace(logs, pid)

    data["task"]["infos"] = task_infos
    data["task"]["not_ready_tasks"] = submit_no_ready_tasks
    data["task"]["not_exec_tasks"] = ready_no_exec_tasks
    data["task"]["not_finish_taks"] = exec_no_done_tasks

    return data, logs_supplement


def write_infos(out_dir, logs, data):
    """
    write process results
    """
    if not os.path.exists(out_dir):
        os.mkdir(out_dir)
    else:
        del_list = os.listdir(out_dir)
        for f in del_list:
            file_path = os.path.join(out_dir, f)
            if os.path.isfile(file_path):
                os.remove(file_path)

    # write recovered trace
    if logs is not None:
        with os.fdopen(
                os.open(out_dir + "/trace_refine.ftrace", os.O_WRONLY | os.O_CREAT | os.O_EXCL, stat.S_IWUSR | stat.S_IRUSR),
                'w') as file:
            file.writelines(logs)
            file.close()

    # write summary info
    with os.fdopen(
            os.open(out_dir + "/summary.txt", os.O_WRONLY | os.O_CREAT | os.O_EXCL,
                    stat.S_IWUSR | stat.S_IRUSR), 'w') as file:
        lines = print_summary(data)
        file.writelines(lines)
        file.close()

    # write thread info
    for tname in data["thread"]["worker"]["S"].keys():
        with os.fdopen(
                os.open("%s/%s.txt" % (out_dir, tname.replace("/", "_")), os.O_WRONLY | os.O_CREAT | os.O_EXCL,
                        stat.S_IWUSR | stat.S_IRUSR), 'w') as file:
            statistics = data["thread"]["worker"]["S"][tname]["statistics"]
            lines = print_hist(statistics)
            file.writelines(lines)

            switch = data["thread"]["worker"]["S"][tname]["statistics"]["switch_out"]
            lines = print_switch(switch)
            file.writelines(lines)
            file.close()

    # write task info
    with os.fdopen(os.open(out_dir + "/task_info.csv", os.O_WRONLY | os.O_CREAT | os.O_EXCL, stat.S_IWUSR | stat.S_IRUSR), 'w', newline="") as file:
        writer = csv.writer(file)
        writer.writerow(
            ["Label", "UID", "Submit_Timestamp", "Ready_Timestamp", "Exec_Timestamp", "Done_Timestamp", "Submit->Ready(us)", "Ready->Exec(us)",
             "Exec->Done(us)", "Cowait_Occurence", "Cowait_Duration(us)", "Exec_Duration(us)", "Workers"])
        for task_name, task_info in data["task"]["infos"].items():
            for task in task_info:
                writer.writerow([task_name, task["gid"], "%s.%s" % (str(task["submit"])[:-6], str(task["submit"])[-6:]),
                                 "%s.%s" % (str(task["ready"])[:-6], str(task["ready"])[-6:]),
                                 "%s.%s" % (str(task["exec"])[:-6], str(task["exec"])[-6:]),
                                 "%s.%s" % (str(task["done"])[:-6], str(task["done"])[-6:]), str(task["submit_ready"]),
                                 str(task["ready_exec"]), str(task["exec_done"]), len(task["cowait"]),
                                 str(task["cowait_duration"]), str(task["exec_duration"]),
                                 str(list(set(task["exec_pids"])))])
        file.close()

    return


def print_hist(s):
    lines = []
    lines.append("State                 |  Duration        |  Occurrence      |  Average\n")
    lines.append("------------------------------------------------------------------------\n")
    for itm in ["running"]:
        if s[itm]["occurrence"] > 0:
            lines.append("%-22s|  %-13s us|  %-16d|  %.0f us\n" % (
                itm, str(s[itm]["duration"]), s[itm]["occurrence"], s[itm]["average"]))
    lines.append("------------------------------------------------------------------------\n\n\n")
    return lines


def print_switch(w):
    lines = []
    lines.append("Switch Type           |  Switch Times / Total Times\n")
    lines.append("------------------------------------------------------\n")
    t = sum(w.values())
    for k in w.keys():
        lines.append("%-22s|    %10d / %-6d\n" % (k, w[k], t))
    lines.append("------------------------------------------------------\n\n\n")
    return lines


def print_task_info(task_name, tasks):
    lines = []
    lines.append("Task Label: " + task_name + ", Total Count: " + str(len(tasks)) + "\n\n")
    lines.append(" gid     |  submit_tstamp |  submit_ready  |  ready_exec  |  exec_done   | cowait_cnt |  cowait_duration  |  exec_duration  |  exec_tids                              \n")
    lines.append("-------------------------------------------------------------------------------------------------------------------------------------------------------------------\n")

    for task in tasks:
        timestamp = str(task["submit"])
        lines.append(" %-6d  |  %-12s  |  %-8sus    |  %-8sus  |  %-8sus  |  %-8d  |  %-10dus     |  %-10dus   | %-48s\n" % (
                        task["gid"], "%s.%s" % (timestamp[:-6], timestamp[-6:]),
                        str(task["submit_ready"]), str(task["ready_exec"]), str(task["exec_done"]),
                        len(task["cowait"]), task["cowait_duration"], task["exec_duration"],
                        str(list(set(task["exec_pids"])))))
    lines.append("-------------------------------------------------------------------------------------------------------------------------------------------------------------------")
    return lines


def print_summary(data):
    lines = []

    lines.append("Summary:\n")
    lines.append("\n1) Suspect Tasks:\n\n")
    lines.append("tasks_not_ready:" + str(data["task"]["not_ready_tasks"]) + "\n")
    lines.append("tasks_not_execute:" + str(data["task"]["not_exec_tasks"]) + "\n")
    lines.append("tasks_not_finish:" + str(data["task"]["not_finish_taks"]) + "\n")
    lines.append("\n------------------------------------------------------------------------------------------\n\n")
    lines.append("2) Thread Overview:\n\n")
    lines.append("%-16s |   %3d worker    | %3d non-worker  | total            |\n" % (
        "thread num", len(data["thread"]["worker"]["T"].keys()), len(data["thread"]["non-worker"]["T"].keys())))
    lines.append("%-16s |      %3.0f%%       |      %3.0f%%       | %-13d us |\n" % (
        "load ratio",
        100.0 * data["total"]["worker_load"] / data["total"]["all_load"] if
        data["total"]["all_load"] != 0 else 0,
        100.0 * (data["total"]["all_load"] - data["total"]["worker_load"]) / data["total"]["all_load"] if
        data["total"]["all_load"] != 0 else 0,
        data["total"]["all_load"]))
    lines.append("%-16s |      %3.0f%%       |      %3.0f%%       | %-14d   |\n" % (
        "context switch",
        100.0 * data["total"]["worker_switch"] / data["total"]["all_switch"] if
        data["total"]["all_switch"] != 0 else 0,
        100.0 * (data["total"]["all_switch"] - data["total"]["worker_switch"]) / data["total"]["all_switch"] if
        data["total"]["all_switch"] != 0 else 0,
        data["total"]["all_switch"]))
    lines.append("\n------------------------------------------------------------------------------------------\n\n")
    lines.append("3) CPU Usage:\n\n")

    lines.append("cpu usage: worker\n")
    for i in sorted(data["cpu"]["worker"].items(), key=lambda kv: (kv[1], kv[0]), reverse=True):
        lines.append("%3.0f%% core %-3d %8d us\n" % (
            100.0 * i[1] / data["total"]["all_load"] if data["total"]["all_load"] != 0 else 0, i[0], i[1]))
    lines.append("\ncpu usage: non-worker\n")
    for i in sorted(data["cpu"]["non-worker"].items(), key=lambda kv: (kv[1], kv[0]), reverse=True):
        lines.append("%3.0f%% core %-3d %8d us\n" % (
            100.0 * i[1] / data["total"]["all_load"] if data["total"]["all_load"] != 0 else 0, i[0], i[1]))
    lines.append("\n------------------------------------------------------------------------------------------\n\n")

    lines.append("4) Thread Distribution:\n\n")
    lines.append("thread info: %d worker load distribution\n" % len(data["thread"]["worker"]["T"].keys()))
    for i in sorted(data["thread"]["worker"]["T"].items(), key=lambda kv: (kv[1], kv[0]), reverse=True):
        lines.append("%3.0f%% %-24s %8d us\n" % (
            100.0 * i[1] / data["total"]["all_load"] if data["total"]["all_load"] != 0 else 0, i[0], i[1]))

    lines.append("\nthread info: %d non-worker load distribution\n" % len(data["thread"]["non-worker"]["T"].keys()))
    for i in sorted(data["thread"]["non-worker"]["T"].items(), key=lambda kv: (kv[1], kv[0]), reverse=True):
        lines.append("%3.0f%% %-24s %8d us\n" % (
            100.0 * i[1] / data["total"]["all_load"] if data["total"]["all_load"] != 0 else 0, i[0], i[1]))
    lines.append("\n------------------------------------------------------------------------------------------\n\n")

    return lines


def clean_logs(logs):
    """
    split logs that are mixed in same line
    """
    num_line = len(logs)

    i = 0
    while i < num_line:
        log = logs[i]

        if " sched_" in log or " tracing_mark_write" in log:
            match = re.finditer("\S+-\d+\s+\(", log)
            indexs = []
            for m in match:
                indexs.append(m.span()[0])
            if len(indexs) > 1:
                del logs[i]
                for j in range(len(indexs)):
                    begin = indexs[j]
                    end = indexs[j + 1] if j + 1 < len(indexs) else len(log)
                    logs.insert(i + j, log[begin:end])

                num_line += len(indexs) - 1
                i += len(indexs) - 1

        i += 1

    return


def main():
    parser = argparse.ArgumentParser(description="parse")
    parser.add_argument('--file', '-f', type=str, required=True, help="input trace file path")
    parser.add_argument('--pid', '-p', type=int, default=None, help="specify process id for trace analyzing")

    args = parser.parse_args()

    logging.basicConfig(filename="./ffrt_trace_process_log.txt", level=logging.DEBUG,
                        format="%(asctime)s - %(levelname)s - %(message)s", datefmt="%m/%d/%Y %H:%M:%S %p")

    if not os.path.isfile(args.file):
        logging.error("file: %s not exist", args.file)
        exit(1)

    with open(args.file, 'r', encoding="gb18030", errors="ignore") as file:
        logs = file.readlines()
        clean_logs(logs)

        ffrt_process, active_process_map, switch_log_map = extract_active_pid_and_switch_log(logs)

        if args.pid is None:
            if len(ffrt_process) == 0:
                logging.error("not find any process used ffrt automatically, plz assign -pid or -p in args")
                exit(1)
            pid = ffrt_process[0]
        else:
            pid = args.pid

        if pid not in active_process_map.keys():
            logging.error("pid %d is not active in trace", pid)
            exit(1)

        data, logs_supplement = process_trace(logs, pid, active_process_map, switch_log_map)
        logging.info("trace process done")
        write_infos(args.file + "_result", logs_supplement, data)
        logging.info("result saved in directory: %s", args.file)

        file.close()


if __name__ == '__main__':
    main()