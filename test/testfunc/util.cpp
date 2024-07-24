/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util.h"
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <functional>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <pthread.h>
#include <linux/perf_event.h>
#include "internal_inc/osal.h"

using namespace std;

constexpr uint32_t PROC_STATUS_LINE_NUM_MAX = 50;

uint32_t get_proc_memory(pid_t pid)
{
    FILE* fd;
    char line[1024] = {0};
    char node_name[64] = {0};
    char vmrss_name[32] = {0};
    uint32_t vmrss_num = 0;
    sprintf(node_name, "/proc/%d/status", pid);
    fd = fopen(node_name, "r");
    if (fd == nullptr) {
        cout << "open " << node_name << " failed" << endl;
        return 0;
    }

    // VmRSS line is uncertain
    for (int i = 0; i < PROC_STATUS_LINE_NUM_MAX; i++) {
        if (fgets(line, sizeof(line), fd) == nullptr) {
            break;
        }
        if (strstr(line, "VmRSS:") != nullptr) {
            sscanf(line, "%s %d", vmrss_name, &vmrss_num);
            break;
        }
    }
    fclose(fd);
    return vmrss_num;
}
void set_cur_process_cpu_affinity(int cpuid)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) == -1) {
        printf("warning:could not set cpu affinity\n");
    }
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    if (sched_getaffinity(0, sizeof(affinity), &affinity) == -1) {
        printf("warning:could not get cpu affinity\n");
        return;
    }
    int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    printf("cpu_num:%d\n", cpu_num);
    for (int i = 0; i < cpu_num; i++) {
        if (CPU_ISSET(i, &affinity)) {
            printf("this process is running on cpu:%d\n", i);
        }
    }
}
#if (defined FFRT_SUPPORT_AOS)
void set_cur_thread_cpu_affinity(int cpuid)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    int ret = syscall(__NR_sched_setaffinity, gettid(), sizeof(mask), &mask);
    if (ret != 0) {
        printf("warning:could not set cpu affinity\n");
        return;
    }
    printf("this thread:%u is running on cpu:%d\n", GetTid(), cpuid);
}
#else
void set_cur_thread_cpu_affinity(int cpuid)
{
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(cpuid, &mask);
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) == -1) {
        printf("warning:could not set cpu affinity\n");
    }
    cpu_set_t affinity;
    CPU_ZERO(&affinity);
    if (pthread_getaffinity_np(pthread_self(), sizeof(affinity), &affinity) == -1) {
        printf("warning:could not get cpu affinity\n");
        return;
    }
    int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    printf("cpu_num:%d\n", cpu_num);
    for (int i = 0; i < cpu_num; i++) {
        if (CPU_ISSET(i, &affinity)) {
            printf("this thread:%d is running on cpu:%d\n", GetTid(), i);
        }
    }
}
#endif

#ifdef FFRT_PERF_EVENT_ENABLE
static int perf_event_open(struct perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

//  sudo perf stat -e instructions:u ./test_main
int perf_single_event(std::function<void()> func, size_t& count, uint32_t event)
{
    int fd;
    struct perf_event_attr attr = {};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(struct perf_event_attr);
    attr.config = event;
    attr.disabled = 1;
    attr.exclude_kernel = 1; // ignore kernel
    attr.exclude_hv = 1; // ignore hyper-visior

    // pid=0,cpu=-1 current process, all cpu
    fd = perf_event_open(&attr, 0, -1, -1, 0);
    if (fd == -1) {
        printf("perf_event_open fail! errno:%d %s\n", errno, strerror(errno));
        return -1;
    }
    ioctl(fd, PERF_EVENT_IOC_RESET, 0); // reset counter
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0); // start count
    func();
    size_t rcount = read(fd, &count, sizeof(count));
    if (rcount < sizeof(count)) {
        printf("perf data read fail! errno:%d %s\n", errno, strerror(errno));
        return -1;
    }
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    close(fd);
    return 0;
}

int perf_event2(std::function<void()> func, struct perf_event2_read_format& data, uint32_t event1, uint32_t event2)
{
    int fd;
    struct perf_event_attr attr = {};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(struct perf_event_attr);
    attr.config = event1;
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.read_format = PERF_FORMAT_GROUP;

    // pid=0,cpu=-1 current process, all cpu
    fd = perf_event_open(&attr, 0, -1, -1, 0);
    if (fd == -1) {
        printf("perf_event_open fail! errno:%d %s\n", errno, strerror(errno));
        return -1;
    }
    attr = {0};
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = sizeof(struct perf_event_attr);
    attr.config = event2;
    attr.disabled = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.read_format = PERF_FORMAT_GROUP;
    int fd2 = perf_event_open(&attr, 0, -1, fd, 0);
    if (fd2 == -1) {
        printf("perf_event_open fail! errno:%d %s\n", errno, strerror(errno));
        return -1;
    }
    ioctl(fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);

    ioctl(fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);

    func();
    size_t rcount = read(fd, &data, sizeof(struct perf_event2_read_format));
    if (rcount < sizeof(struct perf_event2_read_format)) {
        printf("perf data read fail! errno:%d %s\n", errno, strerror(errno));
        return -1;
    }

    ioctl(fd, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
    close(fd);
    close(fd2);
    return 0;
}

int perf_event_instructions(std::function<void()> func, size_t& count)
{
    return perf_single_event(func, count, PERF_COUNT_HW_INSTRUCTIONS);
}
int perf_event_cycles(std::function<void()> func, size_t& count)
{
    return perf_single_event(func, count, PERF_COUNT_HW_CPU_CYCLES);
}
int perf_event_branch_instructions(std::function<void()> func, size_t& count)
{
    return perf_single_event(func, count, PERF_COUNT_HW_BRANCH_INSTRUCTIONS);
}
int perf_event_branch_misses(std::function<void()> func, size_t& count)
{
    return perf_single_event(func, count, PERF_COUNT_HW_BRANCH_MISSES);
}

#else
int perf_event_instructions(std::function<void()> func, size_t& count)
{
    func();
    count = 0;
    return 0;
}

int perf_event_cycles(std::function<void()> func, size_t& count)
{
    func();
    count = 0;
    return 0;
}

int perf_event_branch_instructions(std::function<void()> func, size_t& count)
{
    func();
    count = 0;
    return 0;
}

int perf_event_branch_misses(std::function<void()> func, size_t& count)
{
    func();
    count = 0;
    return 0;
}
#endif