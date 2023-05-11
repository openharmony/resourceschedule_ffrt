# ffrt 

## 环境配置

## 编译

### x86_linux编译:
编译example用例：
```c
    ./scripts/run_example.sh
```

## API
ffrt提供的接口详见`api/ffrt.h`文件，其中：
1. `submit()`接口用于异步提交任务
2. `wait()`接口用于等待异步执行的任务

接口使用示例详见`examples/`目录。

## LOG配置
1. LOG输出函数可以查看头文件ffrt_log_api.h
2. 提供4个日志级别: FFRT_LOG_ERROR = 0, FFRT_LOG_WARN = 1, FFRT_LOG_INFO = 2, FFRT_LOG_DEBUG = 3, 可通过静态编译宏FFRT_LOG_LEVEL来设置日志级别，默认为ERROR
3. 可通过环境变量FFRT_LOG_LEVEL动态设置ffrt日志级别。示例,设置日志DEBUG级别:
```c
    export FFRT_LOG_LEVEL=3  //3为FFRT_LOG_DEBUG的值
```

## Benchmarks
### 测试场景
1. fork_join：通过构造fork/join执行时间，模拟线程创建和堵塞场景；
2. airaw：通过构造gpu和npu任务，模拟拍照airaw场景；
3. fib：通过构造斐波那契序列，模拟嵌套调用场景；
4. face_story：通过构造人脸数据，模拟人脸检测场景；
### 测试方法
```c
    cd benchmarks
    ./benchmarks count //count表明执行次数
```
### 测试结果
1. 测试数据和分析归档到benchmarks/output/tag_${stamp}/benchmark_${stamp}.svg，其中stamp是最近一次commit提交时间
2. 测试结果已取平均

## 代码自动格式化
执行`./script/auto_format.sh`, 推荐clang-format 9.0及以后版本