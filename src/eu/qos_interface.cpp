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

#ifndef GNU_SOURCE
#define GNU_SOURCE
#endif
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "dfx/log/ffrt_log_api.h"
#include "eu/qos_interface.h"

#ifndef OHOS_STANDARD_SYSTEM
#define GET_TID() 10086
#else
#define GET_TID() syscall(SYS_gettid)
#endif

static int TrivalOpenRtgNode(void)
{
    char fileName[] = "/proc/self/sched_rtg_ctrl";
    int fd = open(fileName, O_RDWR);
    if (fd < 0) {
        FFRT_LOGE("task %d belong to user %d open rtg node failed\n", getpid(), getuid());
    }

    return fd;
}

static int TrivalOpenAuthCtrlNode(void)
{
    char fileName[] = "/dev/auth_ctrl";
    int fd = open(fileName, O_RDWR);
    if (fd < 0) {
        FFRT_LOGE("task %d belong to user %d open auth node failed\n", getpid(), getuid());
    }

    return fd;
}

static int TrivalOpenQosCtrlNode(void)
{
    char fileName[] = "/proc/thread-self/sched_qos_ctrl";
    int fd = open(fileName, O_RDWR);
    if (fd < 0) {
        FFRT_LOGE("task %d belong to user %d open qos node failed\n", getpid(), getuid());
    }

    return fd;
}

int EnableRtg(bool flag)
{
    struct RtgEnableData enableData;
    char configStr[] = "load_freq_switch:1;sched_cycle:1";
    int ret;

    enableData.enable = flag;
    enableData.len = sizeof(configStr);
    enableData.data = configStr;
    int fd = TrivalOpenRtgNode();
    if (fd < 0) {
        return fd;
    }

    ret = ioctl(fd, CMD_ID_SET_ENABLE, &enableData);
    if (ret < 0) {
        FFRT_LOGE("set rtg config enable failed.\n");
    }

    close(fd);

    return 0;
};

int AuthEnable(unsigned int uid, unsigned int uaFlag, unsigned int status)
{
    struct AuthCtrlData data;
    int fd;
    int ret;

    fd = TrivalOpenAuthCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.uid = uid;
    data.rtgUaFlag = uaFlag;
    data.qosUaFlag = AF_QOS_ALL;
    data.status = status;
    data.type = AUTH_ENABLE;

    ret = ioctl(fd, BASIC_AUTH_CTRL_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("auth enable failed for uid %d with status %d\n", uid, status);
    }
    close(fd);
    return ret;
}

int AuthSwitch(unsigned int uid, unsigned int rtgFlag, unsigned int qosFlag, unsigned int status)
{
    struct AuthCtrlData data;
    int fd;
    int ret;

    fd = TrivalOpenAuthCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.uid = uid;
    data.rtgUaFlag = rtgFlag;
    data.qosUaFlag = qosFlag;
    data.status = status;
    data.type = AUTH_SWITCH;

    ret = ioctl(fd, BASIC_AUTH_CTRL_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("auth switch failed for uid %d with status %d\n", uid, status);
    }
    close(fd);
    return ret;
}

int AuthDelete(unsigned int uid)
{
    struct AuthCtrlData data;
    int fd;
    int ret;

    fd = TrivalOpenAuthCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.uid = uid;
    data.type = AUTH_DELETE;

    ret = ioctl(fd, BASIC_AUTH_CTRL_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("auth delete failed for uid %d\n", uid);
    }
    close(fd);
    return ret;
}

int AuthPause(unsigned int uid)
{
    struct AuthCtrlData data;
    int fd;
    int ret;

    fd = TrivalOpenAuthCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.uid = uid;
    data.type = AUTH_SWITCH;
    data.rtgUaFlag = 0;
    data.qosUaFlag = AF_QOS_DELEGATED;
    data.status = AUTH_STATUS_BACKGROUND;

    ret = ioctl(fd, BASIC_AUTH_CTRL_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("auth pause failed for uid %d\n", uid);
    }
    close(fd);
    return ret;
}

int AuthGet(unsigned int uid, unsigned int *uaFlag, unsigned int *status)
{
    struct AuthCtrlData data;
    int fd;
    int ret;

    fd = TrivalOpenAuthCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.uid = uid;
    data.type = AUTH_GET;

    ret = ioctl(fd, BASIC_AUTH_CTRL_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("auth get failed for uid %d\n", uid);
    }
    close(fd);

    *uaFlag = data.rtgUaFlag;
    *status = data.status;

    return ret;
}

int QosApply(unsigned int level)
{
    int tid = GET_TID();
    int ret;

    ret = QosApplyForOther(level, tid);
    return ret;
}

int QosApplyForOther(unsigned int level, int tid)
{
    struct QosCtrlData data;
    int fd;

    int ret;

    fd = TrivalOpenQosCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.level = level;
    data.type = QOS_APPLY;
    data.pid = tid;

    ret = ioctl(fd, QOS_CTRL_BASIC_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("qos apply failed for task %d\n", tid);
    }
    close(fd);
    return ret;
}

int QosLeave(void)
{
    struct QosCtrlData data;
    int fd;
    int ret;

    fd = TrivalOpenQosCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.type = QOS_LEAVE;
    data.pid = GET_TID();

    ret = ioctl(fd, QOS_CTRL_BASIC_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("qos leave failed for task %d\n", getpid());
    }

    close(fd);
    return ret;
}

int QosLeaveForOther(int tid)
{
    struct QosCtrlData data;
    int fd;
    int ret;

    fd = TrivalOpenQosCtrlNode();
    if (fd < 0) {
        return fd;
    }

    data.type = QOS_LEAVE;
    data.pid = tid;

    ret = ioctl(fd, QOS_CTRL_BASIC_OPERATION, &data);
    if (ret < 0) {
        FFRT_LOGE("qos leave failed for task %d\n", tid);
    }
    close(fd);
    return ret;
}

int QosPolicy(struct QosPolicyDatas *policyDatas)
{
    int fd;
    int ret;

    fd = TrivalOpenQosCtrlNode();
    if (fd < 0) {
        return fd;
    }

    ret = ioctl(fd, QOS_CTRL_POLICY_OPERATION, policyDatas);
    if (ret < 0) {
        FFRT_LOGE("set qos policy failed for task %d\n", getpid());
    }

    close(fd);
    return ret;
}

Func_affinity funcAffinity = nullptr;
void setFuncAffinity(Func_affinity func)
{
    funcAffinity = func;
}

Func_affinity getFuncAffinity()
{
    return funcAffinity;
}

Func_priority funcPriority = nullptr;
void setFuncPriority(Func_priority func)
{
    funcPriority = func;
}

Func_priority getFuncPriority()
{
    return funcPriority;
}