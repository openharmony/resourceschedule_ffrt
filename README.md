# 并发编程框架 FFRT

- [简介](#简介)
- [目录](#目录)
- [约束](#约束)
- [编译构建](#编译构建)
  - [Linux 编译](#linux-编译)
- [LOG 配置](#log-配置)
- [Benchmarks](#benchmarks)
  - [测试场景](#测试场景)
  - [测试方法](#测试方法)
  - [测试结果](#测试结果)

## 简介

FFRT: Function Flow Runtime，一种并发编程框架，提供以数据依赖的方式构建异步并发任务的能力；包括数据依赖管理、任务执行器、系统事件处理等。并采用基于协程的任务执行方式，可以提高任务并行度、提升线程利用率、降低系统线程总数；充分利用多核平台的计算资源，保证系统对所有资源的集约化管理。最终解决系统线程资源滥用问题，打造极致用户体验。

功能介绍详见： [FFRT 用户指南](docs/user_guide.md)

## 目录

```plain

├── benchmarks                  # 性能对比测试用例
├── docs                        # 用户指南
├── examples                    # 使用案例
├── interfaces                  # 对外接口目录
│   └── kits
│       ├── c
│       └── cpp
├── scripts
├── src
│   ├── core                    # 依赖管理模块
│   ├── dfx                     # 维测功能
│   │   ├── bbox                # 黑匣子功能实现
│   │   ├── log                 # 日志功能
│   │   └── trace               # trace 功能
│   ├── eu                      # 执行单元
│   ├── internal_inc            # 对内接口目录
│   ├── queue
│   ├── sched
│   ├── sync
│   └── util
├── test
└── tools
    └── ffrt_trace_process
```

## 约束

## 编译构建

### Linux 编译

> 当前 FFRT 编译依赖三方安全库的功能，需要把三方安全库的代码一并下载到本地。
>
> 三方安全库代码仓：[third_party_bounds_checking_function]<https://gitee.com/openharmony/third_party_bounds_checking_function>>
>
> 注意：在 FFRT 代码同目录新建文件夹 `third_party`，再将安全库代码放到 `third_party` 下，文件夹命名要求为 `bounds_checking_function`.
>
> 文件结构示意如下：

```plain

├── resourceschedule_ffrt
├── third_party
│   └── bounds_checking_function
```

> 再进入到 ffrt 代码路径下，执行 scripts 下的 shell 脚本 (`sh scripts/run_example.sh`)。

## LOG 配置

1. LOG 输出函数可以查看头文件 `ffrt_log_api.h`
2. 提供 4 个日志级别：`FFRT_LOG_ERROR = 0`， `FFRT_LOG_WARN = 1`，`FFRT_LOG_INFO = 2`，`FFRT_LOG_DEBUG = 3`，可通过静态编译宏 `FFRT_LOG_LEVEL` 来设置日志级别，默认为 `ERROR`
3. 可通过环境变量 `FFRT_LOG_LEVEL` 动态设置 FFRT 日志级别。示例，设置日志 DEBUG 级别：

```shell
    export FFRT_LOG_LEVEL=3  # 3 为 FFRT_LOG_DEBUG 的值
```

## Benchmarks

### 测试场景

1. `fork_join`：通过构造 fork/join 执行时间，模拟线程创建和堵塞场景；
2. `fib`：通过构造斐波那契序列，模拟嵌套调用场景；
3. `face_story`：通过构造人脸数据，模拟人脸检测场景；

### 测试方法

```shell
cd benchmarks
./benchmarks count  # count 表明执行次数
```

### 测试结果

1. 测试数据和分析归档到 `benchmarks/output/tag_${stamp}/benchmark_${stamp}.svg`，其中 `stamp` 是最近一次 commit 提交时间
2. 测试结果已取平均
