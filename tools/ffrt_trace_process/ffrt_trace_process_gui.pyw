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

import os, shutil, stat
import tkinter
import tkinter.filedialog
from tkinter import *
from tkinter import ttk
from tkinter.messagebox import showinfo
from ffrt_trace_process import *

class TraceParserUI(Tk):
    def __init__(self):
        # User Interface Window Define
        self.window = tkinter.Tk()
        self.window.title('FFRT Trace分析辅助工具')
        self.window.geometry('1320x640')
        self.window.resizable(0, 0)

        # Trace Filepath
        self.trace_file = None
        # Trace Logs
        self.logs = None
        self.logs_supplement = None
        # Process
        self.pid = None
        self.curr_pid = None
        self.pid_list = []
        self.ffrt_found = False
        self.active_process_map = None
        self.switch_log_map = None
        self.D = None
        # Thread
        self.tid = None
        self.tid_list = []
        self.tname_list = []
        # Task
        self.task_name = None
        self.task_name_list = []
        self.task_infos = None

        # Start Main Activity
        self.init_window()
        self.window.mainloop()


    def reset_content(self):
        self.D = None
        self.tid_list = []
        self.tname_list = []

        self.ffrt_tid_check['value'] = ('--查看线程信息--')
        self.ffrt_tid_check.current(0)
        self.ffrt_taskname_check['value'] = ('--查看task信息--')
        self.ffrt_taskname_check.current(0)
        self.text_output.delete(0.0, tkinter.END)
        self.statistics_output.delete(0.0, tkinter.END)

        return


    def choose_trace_file(self):
        filename = tkinter.filedialog.askopenfilename()

        if not os.path.isfile(filename):
            showinfo(
                title='Warning',
                message="文件不存在：" + filename
            )
            return

        self.ffrt_pid_check['value'] = ('--下拉选择进程--')
        self.ffrt_pid_check.current(0)
        self.curr_pid = None
        self.reset_content()
        self.text_output.insert(tkinter.INSERT, "正在解析文件：" + filename.split('/')[-1] + "\n")
        self.text_output.update()

        # open trace file
        self.trace_file = filename
        self.logs = open(self.trace_file, 'r', encoding="gb18030", errors="ignore").readlines()
        clean_logs(self.logs)

        # find ffrt processes
        ffrt_process, self.active_process_map, self.switch_log_map = extract_active_pid_and_switch_log(self.logs)
        if len(ffrt_process) > 0:
            self.pid_list = ffrt_process
            self.ffrt_found = True
            self.text_output.insert(tkinter.INSERT, "解析完成：共检测到%d个进程，其中%d个FFRT相关进程：\n\n\n" %
                                    (len(self.active_process_map.keys()), len(self.pid_list)))
        else:
            self.pid_list = list(self.active_process_map.keys())
            self.ffrt_found = False
            self.text_output.insert(tkinter.INSERT, "解析完成：共检测到%d个进程，未发现FFRT相关进程：\n\n\n" %
                                    len(self.active_process_map.keys()))

        pid_list_shown = copy.deepcopy(self.pid_list)
        pid_list_shown.insert(0, self.ffrt_pid_check['value'][0])
        self.ffrt_pid_check['value'] = pid_list_shown

        return


    def on_select_pid(self, event):
        if self.pid.get() == '--下拉选择进程--':
            return

        pid = int(self.pid.get())
        if self.curr_pid != pid:
            self.curr_pid = pid
            self.reset_content()

            self.D, self.logs_supplement = process_trace(self.logs, pid, self.active_process_map, self.switch_log_map)
            self.tid_list = list(self.active_process_map[pid].keys())
            self.tname_list = list(self.active_process_map[pid].values())
            self.text_output.insert(tkinter.INSERT, "检测到进程%d内共%d个线程\n" % (pid, len(self.tname_list)))
            tnames_shown = list(self.tname_list)
            tnames_shown.insert(0, self.ffrt_tid_check['value'][0])
            self.ffrt_tid_check['value'] = tnames_shown

            task_names = list(self.D["task"]["infos"].keys())
            self.text_output.insert(tkinter.INSERT, "检测进程%d内共%d种task类型\n" % (pid, len(task_names)))
            task_names.insert(0, self.ffrt_taskname_check['value'][0])
            self.ffrt_taskname_check['value'] = task_names

        self.statistics_output.delete(0.0, tkinter.END)
        lines = print_summary(self.D)
        for line in lines:
            self.statistics_output.insert(tkinter.INSERT, line)

        return


    def on_select_tid(self, event):
        if self.tid.get() == '--查看线程信息--':
            self.statistics_output.delete(0.0, tkinter.END)
            return

        tname = self.tid.get()

        self.statistics_output.delete(0.0, tkinter.END)
        type = None
        if tname in self.D["thread"]["worker"]["S"].keys():
            type = "worker"
        else:
            type = "non-worker"
        lines = print_hist(self.D["thread"][type]["S"][tname]["statistics"])
        for line in lines:
            self.statistics_output.insert(tkinter.INSERT, line)
        lines = print_switch(self.D["thread"][type]["S"][tname]["statistics"]["switch_out"])
        for line in lines:
            self.statistics_output.insert(tkinter.INSERT, line)
        self.statistics_scroll.config(command=self.statistics_output.yview)

        return


    def on_select_task_name(self, event):
        if self.task_name.get() == '--查看task信息--':
            self.statistics_output.delete(0.0, tkinter.END)
            return

        task_name = str(self.task_name.get())

        self.statistics_output.delete(0.0, tkinter.END)
        lines = print_task_info(task_name, self.D["task"]["infos"][task_name])
        for line in lines:
            self.statistics_output.insert(tkinter.INSERT, line)

        return


    def save_result(self):
        if self.ffrt_found is True:
            out_dir = self.trace_file + "_result"
            if not os.path.exists(out_dir):
                os.mkdir(out_dir)
            else:
                shutil.rmtree(out_dir)
                os.mkdir(out_dir)

            for pid in self.pid_list:
                self.D, _ = process_trace(self.logs, pid, self.active_process_map, self.switch_log_map)
                write_infos(os.path.join(out_dir, str(pid)), None, self.D)

            logs_supplement = self.logs
            for pid in self.pid_list:
                _, _, _, _, logs_supplement = parse_and_convert_task_trace(logs_supplement, pid)

            with os.fdopen(
                    os.open(out_dir + "/trace_refine.ftrace", os.O_WRONLY | os.O_CREAT | os.O_EXCL,
                            stat.S_IWUSR | stat.S_IRUSR),
                    'w') as file:
                file.writelines(logs_supplement)
                file.close()

            self.text_output.insert(tkinter.INSERT, "解析结果保存至: %s\n" % out_dir)
        else:
            self.text_output.insert(tkinter.INSERT, "未检测到FFRT进程，无效操作\n")

        return


    def init_window(self):
        # Frame00
        self.frame00 = Frame(self.window, width=1280, height=200)
        self.frame00.columnconfigure(0, weight=1)
        self.frame00.rowconfigure(0, weight=1)

        # Frame001
        self.frame001 = Frame(self.frame00, width=50, height=200)

        self.btn_choose_trace_file = Button(self.frame001, text="选择trace文件", bg="lightgreen", width=20, height=2, command=self.choose_trace_file)
        self.btn_choose_trace_file.grid(column=0, row=0, pady=10, padx=0)

        self.btn_save_csv = Button(self.frame001, text="保存解析结果", bg="lightblue", width=20, height=2, command=self.save_result)
        self.btn_save_csv.grid(column=0, row=1, pady=20, padx=0)

        self.frame001.grid(column=0, row=0, pady=0, padx=0)

        # Frame002
        self.frame002 = Frame(self.frame00, width=50, height=200)

        self.pid = tkinter.StringVar()
        self.ffrt_pid_check = ttk.Combobox(self.frame002, textvariable=self.pid)
        self.ffrt_pid_check.grid(column=0, row=0, pady=0, padx=0)
        self.ffrt_pid_check['value'] = ('--下拉选择进程--')
        self.ffrt_pid_check['state'] = 'readonly'
        self.ffrt_pid_check.bind('<<ComboboxSelected>>', self.on_select_pid)
        self.ffrt_pid_check.current(0)

        self.frame002.grid(column=1, row=0, pady=0, padx=50)

        # Frame003
        self.frame003 = Frame(self.frame00, width=200, height=200)

        self.tid = tkinter.StringVar()
        self.ffrt_tid_check = ttk.Combobox(self.frame003, textvariable=self.tid)
        self.ffrt_tid_check.grid(column=0, row=1, pady=25, padx=0)
        self.ffrt_tid_check['value'] = ('--查看线程信息--')
        self.ffrt_tid_check['state'] = 'readonly'
        self.ffrt_tid_check.bind('<<ComboboxSelected>>', self.on_select_tid)
        self.ffrt_tid_check.current(0)

        self.task_name = tkinter.StringVar()
        self.ffrt_taskname_check = ttk.Combobox(self.frame003, textvariable=self.task_name)
        self.ffrt_taskname_check.grid(column=0, row=2, pady=25, padx=0)
        self.ffrt_taskname_check['value'] = ('--查看task信息--')
        self.ffrt_taskname_check['state'] = 'readonly'
        self.ffrt_taskname_check.bind('<<ComboboxSelected>>', self.on_select_task_name)
        self.ffrt_taskname_check.current(0)

        self.frame003.grid(column=2, row=0, pady=0, padx=50)

        # Frame004
        self.frame004 = Frame(self.frame00, width=70, height=200)

        self.text_output = Text(self.frame004, height=10, width=70)
        self.text_output.grid(column=0, row=0, pady=0, padx=0)

        self.frame004.grid(column=3, row=0, pady=0, padx=0)

        self.frame00.grid(column=0, row=0, columnspan=3, padx=10)
        self.frame00.grid_propagate(0)

        # Frame10
        self.frame10 = Frame(self.window, bd=5, width=1280, height=330, relief="groove")
        self.frame10.columnconfigure(0, weight=1)
        self.frame10.grid(column=0, row=1, columnspan=3, padx=20)
        self.frame10.grid_propagate(0)

        self.statistics_scroll = ttk.Scrollbar(self.frame10)
        self.statistics_scroll.grid(column=1, row=0, sticky=NS)
        self.statistics_output = Text(self.frame10, yscrollcommand=self.statistics_scroll.set)
        self.statistics_output.grid(column=0, row=0, sticky=NSEW)

if __name__ == '__main__':
    TraceParserUI()