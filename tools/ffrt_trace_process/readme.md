FFRT Trace工具包，目前包含两类工具：
## 1、trace复原工具
主要针对因为ffrt改造，导致原线程/任务的trace被拆分打散在各个ffrt线程上的可视化问题。

此工具的脚本：
- ffrt_trace_recover.py

使用方法：
~~~
# 使用脚本复原trace，会在输入文件同目录下复原一个以_ffrt_recover后缀的新文件
python3 ffrt_trace_recover.py -f in_trace.ftrace
~~~

## 2、trace分析工具
主要包含分析内容：线程运行信息分析、task生命周期分析、task状态计数统计

此工具的脚本：
- ffrt_trace_process.py，分析脚本
- ffrt_trace_process_gui.pyw，可视化界面

使用方法：

1）ffrt_trace_process.py的输入参数为两个
- --file或-f，必需，输入的trace文件路径
- --pid或-p，可选，指定处理的进程号（默认处理第一个ffrt相关进程）

执行示例如下：
~~~
python3 ffrt_trace_process.py --file in_trace.ftrace
~~~

脚本会将处理结果保存在文件同级目录下，以"_result"结尾的文件夹内，解析结果包含：
- 任务生命周期统计：task_info.csv
- 线程运行信息统计：thread-name.txt
- 进程级信息统计：summary.txt
- 优化后的trace文件，内部增加了task状态计数统计，用于perfetto可视化

2）ffrt_trace_process_gui.pyw用户界面使用

本地环境中配置python3，可以直接双击ffrt_trace_process_gui.pyw执行，提供可视化操作界面。

~~~
1）点击选择trace文件

2）进程id下拉列表中，选择进程号进行解析：
- 如果工具检测到使用FFRT的进程，那么下拉列表只展示FFRT相关进程
- 如果工具没有检测到FFRT相关进程，那么下拉列表中会展示所有进程

3）解析完成后，线程id下拉列表可选择查看线程级信息，任务id下拉列表可选择查看任务级信息：
- 线程级信息目前提供running时间、线程切换分类
- 任务级信息目前提供任务的生命周期信息

4）保存解析结果，保存目录同ffrt_trace_process.py脚本一致
~~~