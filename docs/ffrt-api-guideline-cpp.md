# Function Flow Runtime C++ API

## 任务管理

### task_attr

#### 声明

```cpp
class task_attr;
```

#### 描述

任务的属性描述，在提交普通任务或者队列任务时，可以通过`task_attr`来配置其属性。

#### 方法

##### set task name

```cpp
task_attr& task_attr::name(const char* name);
```

参数

- `name`：用户指定的任务名称。

返回值

- `task_attr`对象的引用。

描述

- 设置任务的名称，名称是用于维测信息打印的一种有效信息。

##### get task name

```cpp
const char* task_attr::name() const;
```

返回值

- 任务的名称。

描述

- 获取设置的任务名称。

##### set task qos

```cpp
task_attr& task_attr::qos(qos qos_);
```

参数

- `qos_`：QoS等级。

返回值

- `task_attr`对象的引用。

描述

- 设置任务的QoS等级，QoS等级影响任务执行时的系统资源供给。不设置QoS的情况下，队列任务默认继承队列的QoS等级，普通任务默认设置为`qos_default`。

##### get task qos

```cpp
int task_attr::qos() const;
```

返回值

- QoS等级。

描述

- 获取设置的QoS等级。

##### set task delay

```cpp
task_attr& task_attr::delay(uint64_t delay_us);
```

参数

- `delay_us`：调度延迟，单位为微秒。

返回值

- `task_attr`对象的引用。

描述

- 设置任务的调度延迟，任务会在延迟间隔之后才调度执行。不设置的情况下，默认延迟为零。
- 注意：设置任务的调度延迟后，任务的输入输出依赖关系不再生效。

##### get task delay

```cpp
uint64_t task_attr::delay() const;
```

返回值

- 调度延迟。

描述

- 获取设置的调度延迟。

##### set task priority

```cpp
task_attr& task_attr::priority(ffrt_queue_priority_t prio);
```

参数

- `prio`：任务优先级。

返回值

- `task_attr`对象的引用。

描述

- 设置任务的优先级，目前仅并发队列任务支持优先级功能，同一个并发队列中按照优先级顺序来调度任务。不设置的情况下，任务默认优先级为`ffrt_queue_priority_low`。

##### get task priority

```cpp
ffrt_queue_priority_t task_attr::priority() const;
```

返回值

- 任务优先级。

描述

- 获取设置的优先级。

##### set task stack_size

```cpp
task_attr& task_attr::stack_size(uint64_t size);
```

参数

- `size`：协程栈大小，单位为字节。

返回值

- `task_attr`对象的引用。

描述

- 设置任务的协程栈大小，影响任务执行过程中最大的调用栈使用空间上限。在不设置的情况下，默认的协程栈大小为1MB。

##### get task stack_size

```cpp
uint64_t task_attr::stack_size() const;
```

返回值

- 协程栈大小。

描述

- 获取设置的协程栈大小。

##### set task timeout

```cpp
task_attr& task_attr::timeout(uint64_t timeout_us);
```

参数

- `timeout_us`：任务的调度超时时间，单位为微秒。

返回值

- `task_attr`对象的引用。

描述

- 设置任务的调度超时时间，仅针对队列任务生效，当队列任务超过该时间还未调度执行时，打印告警信息。不设置的情况下，默认没有调度超时限制。

##### get task timeout

```cpp
uint64_t task_attr::timeout() const;
```

返回值

- 调度超时时间。

描述

- 获取设置的超时时间。

#### 样例

```cpp
// 提交一个普通任务，其名称为"sample_task"，QoS等级为background，调度时延为1ms，协程栈大小为2MB
ffrt::submit([]() { std::cout << "hello world!" << std::endl; }, ffrt::task_attr().name("sample_task").qos(ffrt::qos_background).delay(1000).stack_size(2 * 1024 * 1024));

// 提交一个并发队列任务，其优先级为high，调度超时时间为100ms
ffrt::queue sample_queue(queue_concurrent, "sample_queue");
sample_queue.submit([]() { std::cout << "hello world!" << std::endl; }, ffrt::task_attr().priority(ffrt::ffrt_queue_priority_high).timeout(100 * 1000));
```

### task_handle

#### 声明

```cpp
class task_handle;
```

#### 描述

任务的句柄，其作用包括：

- 任务生命周期管理，句柄绑定的任务，在句柄存在的生命周期内，都是有效的。
- 配合入参为`task_handle`的方法使用，例如任务同步、任务取消、任务查询等。

#### 方法

##### get_id

```cpp
uint64_t task_handle::get_id() const;
```

返回值

- 任务的id标识。

描述

- 获取`task_handle`对应任务的id。

#### 样例

```cpp
// 通过任务句柄同步一个普通任务完成
ffrt::task_handle t = ffrt::submit_h([]() { std::cout << "hello world!" << std::endl; });
ffrt::wait({t});

// 通过任务句柄同步一个队列任务完成
ffrt::queue* testQueue = new ffrt::queue("test_queue");
task_handle t = testQueue->submit_h([]() { std::cout << "hello world!" << std::endl; });
testQueue->wait(t);

// 通过任务句柄取消一个队列任务
ffrt::queue* testQueue = new ffrt::queue("test_queue");
task_handle t = testQueue->submit_h([]() { std::cout << "hello world!" << std::endl; });
int ret = testQueue->cancel(t);
```

### create_function_wrapper

#### 声明

```cpp
template<class T>
ffrt_function_header_t* create_function_wrapper(T&& func, ffrt_function_kind_t kind = ffrt_function_kind_general);
```

#### 参数

- `func`：任务执行的函数闭包。
- `kind`：提交普通任务选择`ffrt_function_kind_general`，提交队列任务选择`ffrt_function_kind_queue`。

#### 返回值

- 返回任务执行器的指针，描述了该CPU任务如何执行和销毁。

#### 描述

构建FFRT任务的封装函数，此代码为公共代码与具体业务场景无关，`submit`和`queue::submit`函数都已封装此函数，使用FFRT C++ API时无需关心此接口。

#### 样例

具体样例参见[开发步骤](ffrt-development-guideline.md#开发步骤)。

### submit

#### 声明

```cpp
static void submit(std::function<void()>&& func, const task_attr& attr = {});
static void submit(std::function<void()>&& func, std::initializer_list<dependence> in_deps, const task_attr& attr = {});
static void submit(std::function<void()>&& func, std::initializer_list<dependence> in_deps, std::initializer_list<dependence> out_deps, const task_attr& attr = {});
static void submit(std::function<void()>&& func, const std::vector<dependence>& in_deps, const task_attr& attr = {});
static void submit(std::function<void()>&& func, const std::vector<dependence>& in_deps, const std::vector<dependence>& out_deps, const task_attr& attr = {});
static void submit(const std::function<void()>& func, const task_attr& attr = {});
static void submit(const std::function<void()>& func, std::initializer_list<dependence> in_deps, const task_attr& attr = {});
static void submit(const std::function<void()>& func, std::initializer_list<dependence> in_deps, std::initializer_list<dependence> out_deps, const task_attr& attr = {});
static void submit(const std::function<void()>& func, const std::vector<dependence>& in_deps, const task_attr& attr = {});
static void submit(const std::function<void()>& func, const std::vector<dependence>& in_deps, const std::vector<dependence>& out_deps, const task_attr& attr = {});
```

#### 参数

- `func`：任务执行的函数闭包。
- `in_deps`：任务的输入数据依赖。支持初始化列表和`vector`形式。输入数据依赖通常以实际数据的地址表达，也支持`task_handle`作为一种特殊输入依赖。
- `out_deps`：任务的输出数据依赖。支持初始化列表和`vector`形式。输出数据依赖通常以实际数据的地址表达，不支持`task_handle`。
- `attr`：任务的属性设置。

#### 描述

提交一个普通任务，任务支持相关属性设置，在输入依赖解除后任务可调度执行，任务执行完成后解除输出依赖。

#### 样例

```cpp
// 提交一个任务
ffrt::submit([]() { std::cout << "hello world!" << std::endl; });

// 提交一个带属性的任务
ffrt::submit([]() { std::cout << "hello world!" << std::endl; }, ffrt::task_attr().name("sample_task").qos(ffrt::qos_background));

// 提交两个任务，任务间存在Read-Aftter-Write依赖关系
float x = 0.5f, y, z;
ffrt::submit([&]() { y = std::cos(x); }, {&x}, {&y}); // 第一个任务输入依赖x，输出依赖y
ffrt::submit([&]() { z = std::tan(y); }, {&y}, {&z}); // 第二个任务输入依赖y（和第一个任务产生Read-Aftter-Write依赖），输出依赖z

// 提交两个任务，任务间存在Write-Aftter-Write依赖关系
float x = 0.5f, y;
ffrt::submit([&]() { y = std::cos(x); }, {&x}, {&y}); // 第一个任务输入依赖x，输出依赖y
ffrt::submit([&]() { y = std::tan(y); }, {}, {&y}); // 第二个任务输出依赖y（和第一个任务产生Write-After-Write依赖），注意这里y虽然也是输入依赖，但写法上可以省略
ffrt::wait({&y});

// 提交两个任务，任务间存在Write-Aftter-Read依赖关系
float x = 0.5f;
ffrt::submit([&]() { std::cout << x << std::endl; }, {&x}, {}); // 第一个任务输入依赖x
ffrt::submit([&]() { x = 1.0f; }, {}, {&x}); // 第二个任务输出依赖x（和第一个任务产生Write-After-Read依赖）

// 提交两个任务，不存在实际依赖，完全可并发
float x = 0.5f;
ffrt::submit([&]() { std::cout << x << std::endl; }, {&x}, {}); // 第一个任务输入依赖x
ffrt::submit([&]() { std::cout << x * 2 << std::endl; }, {&x}, {}); // 第二个任务输入依赖x（和第一个任务不产生依赖关系）

// 使用vector数组存储依赖作为入参
std::vector<dependence> indeps;
indeps.emplace_back(&x);
std::vector<dependence> outdeps;
outdeps.emplace_back(&y);
outdeps.emplace_back(&z);
ffrt::submit([&]() { y = std::cos(x); z = std::tan(x); }, indeps, outdeps);
```

### submit_h

#### 声明

```cpp
static task_handle submit_h(std::function<void()>&& func, const task_attr& attr = {});
static task_handle submit_h(std::function<void()>&& func, std::initializer_list<dependence> in_deps, const task_attr& attr = {});
static task_handle submit_h(std::function<void()>&& func, std::initializer_list<dependence> in_deps, std::initializer_list<dependence> out_deps, const task_attr& attr = {});
static task_handle submit_h(std::function<void()>&& func, const std::vector<dependence>& in_deps, const task_attr& attr = {});
static task_handle submit_h(std::function<void()>&& func, const std::vector<dependence>& in_deps, const std::vector<dependence>& out_deps, const task_attr& attr = {});
static task_handle submit_h(const std::function<void()>& func, const task_attr& attr = {});
static task_handle submit_h(const std::function<void()>& func, std::initializer_list<dependence> in_deps, const task_attr& attr = {});
static task_handle submit_h(const std::function<void()>& func, std::initializer_list<dependence> in_deps, std::initializer_list<dependence> out_deps, const task_attr& attr = {});
static task_handle submit_h(const std::function<void()>& func, const std::vector<dependence>& in_deps, const task_attr& attr = {});
static task_handle submit_h(const std::function<void()>& func, const std::vector<dependence>& in_deps, const std::vector<dependence>& out_deps, const task_attr& attr = {});
```

#### 参数

- `func`：任务执行的函数闭包。
- `in_deps`：任务的输入数据依赖。支持初始化列表和`vector`形式。输入数据依赖通常以实际数据的地址表达，也支持`task_handle`作为一种特殊输入依赖。
- `out_deps`：任务的输出数据依赖。支持初始化列表和`vector`形式。输出数据依赖通常以实际数据的地址表达，不支持`task_handle`。
- `attr`：任务的属性设置。

#### 返回值

- 任务的句柄`task_handle`。

#### 描述

相比于`submit`接口，增加了任务句柄的返回值。

#### 样例

```cpp
// 提交一个普通任务，并通过句柄同步其完成
ffrt::task_handle t = ffrt::submit_h([]() { std::cout << "hello world!" << std::endl; });
ffrt::wait({t});

// 通过任务句柄来建立依赖关系
int x = 0, y, z;
ffrt::task_handle t1 = ffrt::submit_h([&]() { y = x + 2; }, {&x}, {&y});
ffrt::task_handle t2 = ffrt::submit_h([&]() { z = x + 4; }, {&x}, {&z});
ffrt::submit([&]() { z += y; }, {t1, t2}); // 第三个任务，使用前两个任务的句柄作为输入依赖，建立和前两个任务的依赖关系
```

### wait

#### 声明

```cpp
static void wait();
static void wait(std::initializer_list<dependence> deps);
static void wait(const std::vector<dependence>& deps);
```

#### 参数

- `deps`：需要同步的数据依赖，支持初始化列表或`vector`形式。

#### 描述

`wait`函数分为多种重载形式：

1. 不带参数的`wait`函数，表示同步等待所有前序提交的同级任务完成。
2. 带`deps`参数的`wait`函数，表示同步对应的任务依赖解除。

#### 样例

```cpp
// wait同步一个任务完成
ffrt::submit([]() { std::cout << "hello world!" << std::endl; });
ffrt::wait();

// wait同步多个任务完成
ffrt::submit([]() { std::cout << "this is task1" << std::endl; });
ffrt::submit([]() { std::cout << "this is task2" << std::endl; });
ffrt::submit([]() { std::cout << "this is task3" << std::endl; });
ffrt::wait();

// wait同步一个数据依赖解除
int x;
ffrt::submit([&]() { x = 1; }, {}, {&x});
ffrt::wait({&x});

// wait同步多个数据依赖解除
int x, y, z;
ffrt::submit([&]() { x = 1; }, {}, {&x});
ffrt::submit([&]() { y = 1; }, {}, {&y});
ffrt::submit([&]() { z = 1; }, {}, {&z});
ffrt::wait({&x, &y, &z});

// 在嵌套场景下的wait同步任务
ffrt::submit([]() { std::cout << "this is task1" << std::endl; });
ffrt::submit([]() {
    ffrt::submit([]() { std::cout << "this is task2.1" << std::endl; });
    ffrt::submit([]() { std::cout << "this is task2.2" << std::endl; });
    ffrt::submit([]() { std::cout << "this is task2.3" << std::endl; });
    ffrt::wait(); // 同步三个同级子任务完成（2.1、2.2、2.3），不会同步上一级的任务（task1）
});

// 在嵌套场景下的wait同步数据依赖
ffrt::submit([&]() { x = 1; std::cout << "this is task1" << std::endl; }, {}, {&x});
ffrt::submit([]() {
    ffrt::submit([]() { std::cout << "this is task2.1" << std::endl; });
    ffrt::submit([]() { std::cout << "this is task2.2" << std::endl; });
    ffrt::submit([]() { std::cout << "this is task2.3" << std::endl; });
    ffrt::wait({&x}); // wait同步数据依赖支持跨层级使用，可以同步上一级任务的输出依赖解除（task1）
});
```

### set_worker_stack_size

#### 声明

```cpp
static ffrt_error_t set_worker_stack_size(qos qos_, size_t stack_size);
```

#### 参数

- `qos_`：QoS等级。
- `stack_size`：Worker线程栈大小，单位是字节。

#### 返回值

- 错误码，可参考`ffrt_error_t`枚举。

#### 描述

在开始提交任务前，设置某一组QoS的Worker线程栈大小（Worker线程按QoS分组，组间互不影响，组内线程属性相同）。通常该接口用于用户提交非协程任务且函数栈超过默认上限的场景，不设置时线程栈和OS默认规格一致。

#### 样例

```cpp
// 把qos_default的Worker线程组的线程栈大小设置成2MB
ffrt_error_t ret = set_worker_stack_size(qos_default, 2 * 1024 * 1024);
```

### update_qos

#### 声明

```cpp
static int this_task::update_qos(qos qos_);
```

#### 参数

- `qos_`：QoS等级。

#### 返回值

- 0表示成功，1表示失败。

#### 描述

在任务执行过程中，动态修改任务的QoS等级。注意该接口在任务的函数闭包内使用，修改的是当前正在执行的任务的QoS等级，接口调用会使任务先挂起一次再恢复执行。

#### 样例

```cpp
// 一个qos_background的任务执行过程中动态修改QoS等级
ffrt::submit([]() {
    // ...
    int ret = ffrt::this_task::update_qos(ffrt::qos_user_initiated);
    // ...
}, ffrt::task_attr().qos(ffrt::qos_background));
```

### get_id

#### 声明

```cpp
static uint64_t this_task::get_id();
```

#### 返回值

- 当前任务的id。

#### 描述

获取当前执行任务的id，注意该接口在任务的函数闭包内使用。

#### 样例

```cpp
ffrt::submit([&] {
    uint64_t id = ffrt::this_task::get_id();
});
```

## 任务队列

### queue_attr

#### 声明

```cpp
class queue_attr;
```

#### 描述

用于配置队列的属性，如QoS、超时时间、回调函数和最大并发数。

#### 方法

##### set queue qos

```cpp
queue_attr& queue_attr::qos(qos qos_);
```

参数

- `qos_`：用户指定的QoS等级。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置队列的QoS等级。

##### get queue qos

```cpp
int queue_attr::qos() const;
```

返回值

- 返回当前QoS等级。

描述

- 获取当前属性中设置的QoS等级。

##### set queue timeout

```cpp
queue_attr& queue_attr::timeout(uint64_t timeout_us);
```

参数

- `timeout_us`：队列任务执行超时阈值（微秒）。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置队列的超时时间（以微秒为单位）。

##### get queue timeout

```cpp
uint64_t queue_attr::timeout() const;
```

返回值

- 返回当前超时阈值（微秒）。

描述

- 获取当前属性中设置的超时时间。

##### set queue callback

```cpp
queue_attr& queue_attr::callback(const std::function<void()>& func);
```

参数

- `func`：回调函数。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置检测到队列任务超时后执行的回调函数。
- 不建议在`func`中调用`exit`函数，可能导致未定义行为。
- 任务在`timeout`约定时间点未执行完成，ffrt则会调用该`callback`函数，**需要注意的是：`callback`函数执行时，任务可能正在执行或已执行完。**

##### get queue callback

```cpp
ffrt_function_header_t* queue_attr::callback() const;
```

返回值

- 返回任务执行器的指针，描述了该CPU任务如何执行和销毁。

描述

- 获取当前属性中设置的超时回调函数。

##### set queue max_concurrency

```cpp
queue_attr& queue_attr::max_concurrency(const int max_concurrency);
```

参数

- `max_concurrency`：最大并发数。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置队列的最大并发数（仅支持并发队列）。

##### get queue max_concurrency

```cpp
int queue_attr::max_concurrency() const;
```

返回值

- 返回当前最大并发数。

描述

- 获取当前属性中设置的最大并发数（仅支持并发队列）。

##### set queue thread_mode

```cpp
queue_attr& queue_attr::thread_mode(bool mode)
```

参数

- `bool`设置队列任务运行方式（`true`以线程模式运行, `false`以协程模式运行）。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置队列中的任务是以协程模式还是以线程模式运行。

##### get queue thread_mode

```cpp
bool queue_attr::thread_mode() const
```

返回值

- `true`表示以线程模式运行，`false`表示以协程模式运行。

描述

- 获取队列中的任务是以协程模式还是以线程模式运行。

#### 样例

```cpp
#include "ffrt/cpp/queue.h"

int main()
{
    int x = 0;
    std::function<void()> callbackFunc = [&x]() {
        x++;
    };

    // 创建队列，可设置队列优先级，默认为 default 等级
    ffrt::queue que1("test_1", ffrt::queue_attr().qos(ffrt::qos_utility));
    // 创建队列，可通过设置 timeout 打开队列任务超时监测（默认关闭）
    // 超时会打印 Error 日志并执行用户设置的 callback（可选）
    ffrt::queue que2("test_2", ffrt::queue_attr().timeout(1000).callback(callbackFunc));
    return 0;
}
```

### queue

#### 声明

```cpp
class queue;
```

#### 描述

用于创建和管理队列，支持队列任务的提交、取消、等待和排队任务数量查询。

#### 方法

##### 队列创建

```cpp
queue(const char* name, const queue_attr& attr = {});
queue(const queue_type type, const char* name, const queue_attr& attr = {});
```

参数

- `type`：队列类型（如`queue_serial`或`queue_concurrent`），省略此入参时默认是`queue_serial`。
- `name`：队列名称。
- `attr`：队列属性（可选）。

描述

- 构造函数，创建指定类型和名称的队列。

##### submit

```cpp
void queue::submit(const std::function<void()>& func, const task_attr& attr = {});
void queue::submit(std::function<void()>&& func, const task_attr& attr = {});
```

参数

- `func`：任务函数闭包，支持左值引用和右值引用两个版本。
- `attr`：任务属性（可选）。

描述

- 提交一个任务到队列中。

##### submit_h

```cpp
task_handle queue::submit_h(const std::function<void()>& func, const task_attr& attr = {});
task_handle queue::submit_h(std::function<void()>&& func, const task_attr& attr = {});
```

参数

- `func`：任务函数闭包，支持左值引用和右值引用两个版本。
- `attr`：任务属性（可选）。

返回值

- `task_handle`：返回任务句柄。

描述

- 提交一个任务到队列中，并返回任务句柄。

##### submit_head

```cpp
void queue::submit_head(const std::function<void()>& func, const task_attr& attr = {});
void queue::submit_head(std::function<void()>&& func, const task_attr& attr = {});
```

参数

- `func`：任务函数闭包，支持左值引用和右值引用两个版本。
- `attr`：任务属性（可选）。

描述

- 提交一个任务到队列头部。

##### submit_head_h

```cpp
task_handle queue::submit_head_h(const std::function<void()>& func, const task_attr& attr = {});
task_handle queue::submit_head_h(std::function<void()>&& func, const task_attr& attr = {});
```

参数

- `func`：任务函数闭包，支持左值引用和右值引用两个版本。
- `attr`：任务属性（可选）。

返回值

- `task_handle`：返回任务句柄。

描述

- 提交一个任务到队列头部，并返回任务句柄。

##### cancel

```cpp
int queue::cancel(const task_handle& handle);
```

参数

- `handle`：任务句柄。

返回值

- 返回0表示成功，其他值表示失败。

描述

- 取消一个任务。

##### wait

```cpp
void queue::wait(const task_handle& handle);
```

参数

- `handle`：任务句柄。

描述

- 等待一个任务。

##### get_task_cnt

```cpp
uint64_t queue::get_task_cnt();
```

返回值

- 此队列排队的任务数。

描述

- 获取在队列中排队等待的任务数量。

##### get_main_queue

```cpp
static queue* queue::get_main_queue();
```

返回值

- 主线程队列。

描述

- 获取主线程队列，用于FFRT线程与主线程通信。

#### 样例

```cpp
#include "ffrt/cpp/queue.h"

int main()
{
    // 创建队列，可设置队列优先级，默认为 default 等级
    ffrt::queue que("test_queue", ffrt::queue_attr().qos(ffrt::qos_utility));
    int x = 0;
    // 提交串行任务
    que.submit([&x] { x += 10; });
    // 提交串行任务，并返回任务句柄
    ffrt::task_handle t1 = que.submit_h([&x] { x += 10; });
    // 提交串行任务，设置延时时间 1000us，并返回任务句柄
    ffrt::task_handle t2 = que.submit_h([&x] { x += 10; }, ffrt::task_attr().delay(1000));
    // 等待指定任务执行完成
    que.wait(t1);
    // 取消句柄为 t2 的任务
    que.cancel(t2);

    return 0;
}
```

## 同步原语

### mutex

#### 声明

```cpp
class mutex;
```

#### 描述

- FFRT提供的类似`std::mutex`的性能实现。
- 该功能能够避免传统的`std::mutex`在抢不到锁时陷入内核的问题，在使用得当的条件下将会有更好的性能。

#### 方法

##### try_lock

```cpp
bool mutex::try_lock();
```

返回值

- 获取锁是否成功。

描述

- 尝试获取FFRT互斥锁。

##### lock

```cpp
void mutex::lock();
```

描述

- 获取FFRT互斥锁。

##### unlock

```cpp
void mutex::unlock();
```

描述

- 释放FFRT互斥锁。

#### 样例

```cpp
#include <chrono>
#include <thread>
#include "ffrt/cpp/sleep.h"
#include "ffrt/cpp/mutex.h"
#include "ffrt/cpp/task.h"

int main()
{
    int x = 0;
    int y = 0;
    ffrt::mutex lock;

    auto thread1Func = [&]() {
        ffrt::submit([&]() {
            ffrt::this_task::sleep_for(std::chrono::milliseconds(10));
            while (true) {
                if (lock.try_lock()) {
                    lock.unlock();
                    return;
                } else {
                    y++;
                    ffrt::this_task::sleep_for(std::chrono::milliseconds(10));
                }
            }
            }, {}, {}, ffrt::task_attr().name("t2"));
        ffrt::wait();
    };

    auto thread2Func = [&]() {
        ffrt::submit([&]() {
            lock.lock();
            ffrt::this_task::sleep_for(std::chrono::milliseconds(50));
            x++;
            lock.unlock();
            }, {}, {}, ffrt::task_attr().name("t1"));
        ffrt::wait();
    };

    std::thread t1(thread1Func);
    std::thread t2(thread2Func);
    t1.join();
    t2.join();

    return 0;
}
```

### shared_mutex

#### 声明

```cpp
class shared_mutex;
```

#### 描述

- FFRT提供类似`std::shared_mutex`的性能实现，在使用中要区分读锁和写锁。
- 该功能能够避免传统的`std::shared_mutex`在进入睡眠后不释放线程的问题，在使用得当的条件下将会有更好的性能。

#### 方法

##### try_lock

```cpp
bool shared_mutex::try_lock();
```

返回值

- 获取锁是否成功。

描述

- 尝试获取FFRT写锁。

##### lock

```cpp
void shared_mutex::lock();
```

描述

- 获取FFRT写锁。

##### unlock

```cpp
void shared_mutex::unlock();
```

描述

- 释放FFRT写锁。

##### lock_shared

```cpp
void shared_mutex::lock_shared();
```

描述

- 获取FFRT读锁。

##### try_lock_shared

```cpp
void shared_mutex::try_lock_shared();
```

描述

- 尝试获取FFRT读锁。

##### unlock_shared

```cpp
void shared_mutex::unlock_shared();
```

描述

- 释放FFRT读锁。

#### 样例

```cpp
#include <chrono>
#include "ffrt/cpp/task.h"
#include "ffrt/cpp/shared_mutex.h"
#include "ffrt/cpp/sleep.h"

int main()
{
    int x = 0;
    ffrt::shared_mutex smut;

    ffrt::submit([&]() {
        smut.lock();
        ffrt::this_task::sleep_for(std::chrono::milliseconds(10));
        x++;
        smut.unlock();
        }, {},{});

    ffrt::submit([&]() {
        ffrt::this_task::sleep_for(std::chrono::milliseconds(2));
        smut.lock_shared();
        smut.unlock();
        }, {},{});

    ffrt::submit([&]() {
        ffrt::this_task::sleep_for(std::chrono::milliseconds(2));
        if(smut.try_lock()){
            x++:
            smut.unlock();
        }
        }, {},{});

    ffrt::submit([&]() {
        ffrt::this_task::sleep_for(std::chrono::milliseconds(2));
        if(smut.try_lock_shared()){
            smut.unlock_shared();
        }
        }, {},{});

    return 0;
}
```

### recursive_mutex

#### 声明

```cpp
class recursive_mutex;
```

#### 描述

- FFRT提供的类似`std::recursive_mutex`的性能实现。
- 该功能能够避免传统的`std::recursive_mutex`在抢不到锁时陷入内核的问题，在使用得当的条件下将会有更好的性能。

#### 方法

##### try_lock

```cpp
bool recursive_mutex::try_lock();
```

返回值

- 获取锁是否成功。

描述

- 尝试获取FFRT递归锁。

##### lock

```cpp
void recursive_mutex::lock();
```

描述

- 获取FFRT递归锁。

##### unlock

```cpp
bool recursive_mutex::unlock();
```

描述

- 释放FFRT递归锁。

#### 样例

```cpp
#include "ffrt/cpp/mutex.h"
#include "ffrt/cpp/task.h"

int main()
{
    ffrt::recursive_mutex lock;
    int sum = 0;
    ffrt::submit([&]() {
        lock.lock();
        lock.try_lock();
        sum++;
        lock.lock();
        lock.try_lock();
        sum++;
        lock.unlock();
        lock.unlock();
        lock.unlock();
        lock.unlock();
        }, {}, {});
    ffrt::wait();
    return 0;
}
```

### condition_variable

#### 声明

```cpp
enum class cv_status { no_timeout, timeout };

class condition_variable;
```

#### 描述

- 该接口支持在FFRT任务内部调用，也支持在FFRT任务外部调用。
- FFRT 提供的类似`std::condition_variable`的性能实现。
- 该功能能够避免传统的`std::condition_variable`在条件不满足时陷入内核的问题，在使用得当的条件下将会有更好的性能。

#### 方法

##### wait_until

```cpp
template <typename Clock, typename Duration, typename Pred>
bool condition_variable::wait_until(std::unique_lock<mutex>& lk, const std::chrono::time_point<Clock, Duration>& tp, Pred&& pred) noexcept;
```

参数

- `lk`：mutex互斥量。
- `tp`：等待时间。
- `pred`：检查是否等待函数。

返回值

- 是否满足判断条件。

描述

- 该方法用于在指定时间点之前，阻塞当前任务并等待一个条件变量的通知，直到满足给定的谓词（Predicate）或者超时。

##### wait_for

```cpp
template <typename Rep, typename Period, typename Pred>
bool condition_variable::wait_for(std::unique_lock<mutex>& lk, const std::chrono::duration<Rep, Period>& sleepTime, Pred&& pred);
```

参数

- `lk`：mutex互斥量。
- `sleepTime`：等待时间。
- `pred`：检查是否等待函数。

返回值

- 是否满足判断条件。

描述

- 该方法用于在指定时间点之前，阻塞当前任务并等待一个条件变量的通知，直到满足给定的谓词（Predicate）或者超时。

##### wait

```cpp
template <typename Pred>
void condition_variable::wait(std::unique_lock<mutex>& lk, Pred&& pred);
```

参数

- `lk`：mutex互斥量。
- `pred`：检查是否等待函数。

描述

- 该方法用于等待某个条件变量的通知，直到满足给定的谓词（Predicate）。

##### notify_one

```cpp
void condition_variable::notify_one() noexcept;
```

描述

- 该方法用于通知一个正在等待该条件变量的线程，唤醒它继续执行。具体哪个线程被唤醒，由操作系统的调度器决定。

##### notify_all

```cpp
void condition_variable::notify_all() noexcept;
```

描述

- 该方法用于通知所有正在等待该条件变量的线程，唤醒它继续执行。这些线程会争夺获取锁并继续执行。

#### 样例

```cpp
#include <chrono>
#include <unistd.h>
#include <thread>
#include "ffrt/cpp/condition_variable.h"
#include "ffrt/cpp/mutex.h"
#include "ffrt/cpp/task.h"

int main()
{
    const int sleepTime = 50 * 1000;
    const int checkDelayTime = 10 * 1000;
    const std::chrono::milliseconds waitTime = std::chrono::milliseconds(100);
    ffrt::condition_variable cond;
    ffrt::mutex lock_;
    int val = 0;
    int predVal = 0;
    const int lastVal = 2;

    auto threadWaitFunc = [&]() {
        std::unique_lock<ffrt::mutex> lck(lock_); // 使用 std::mutex
        cond.wait_until(lck, std::chrono::steady_clock::now() + waitTime, [&] { return predVal == 1; });
        val = lastVal;
    };

    auto threadNotifyFunc = [&]() {
        val = 1;
        usleep(sleepTime);
        predVal = 1;
        cond.notify_one();
    };

    std::thread tWait(threadWaitFunc);
    std::thread tNotify(threadNotifyFunc);
    tWait.join();
    tNotify.join();
    usleep(checkDelayTime);
    return 0;
}
```

## 阻塞原语

### sleep

#### 描述

- 该接口支持在FFRT任务内部调用，也支持在FFRT任务外部调用。
- FFRT提供的类似`std::this_thread::sleep_for`和`std::this_thread::sleep_until`的性能实现。
- 该功能能够避免传统的`std::this_thread::sleep_for`睡眠时陷入内核的问题，在使用得当的条件下将会有更好的性能。
- 该接口调用后实际睡眠时长不小于配置值。

#### 方法

##### sleep_for

```cpp
template <class _Rep, class _Period>
void this_task::sleep_for(const std::chrono::duration<_Rep, _Period>& d);
```

参数

- `d`：要休眠的持续时间。

描述

- 该方法用于让当前任务休眠指定的持续时间。

##### sleep_until

```cpp
template<class _Clock, class _Duration>
void this_task::sleep_until(
    const std::chrono::time_point<_Clock, _Duration>& abs_time);
```

参数

- `abs_time`：绝对时间点，表示任务应该被唤醒的目标时间。

描述

- 该方法用于让当前任务休眠直到指定的绝对时间点。

#### 样例

```cpp
#include "ffrt/cpp/sleep.h"
#include "ffrt/cpp/task.h"

int main()
{
    ffrt::submit([] {
        ffrt::this_task::sleep_for(std::chrono::milliseconds(2000));
    });
    ffrt::wait();
    return 0;
}
```

## 协同原语

### yield

#### 声明

```cpp
static void this_task::yield();
```

#### 描述

- 该接口支持在FFRT任务内部调用，也支持在FFRT任务外部调用。
- 当前任务主动让出CPU执行资源，让其他可以被执行的任务先执行，如果没有其他可被执行的任务，`yield`无效。
- 此函数的确切行为取决于实现，特别是使用中的FFRT调度程序的机制和系统状态。

#### 样例

```cpp
#include "ffrt/cpp/sleep.h"
#include "ffrt/cpp/task.h"

int main()
{
    const std::chrono::milliseconds setTime = std::chrono::milliseconds(5);
    ffrt::submit([&]() {
        ffrt::this_task::yield();
        ffrt::this_task::sleep_for(setTime);
        ffrt::submit([&]() {
            ffrt::this_task::sleep_for(setTime);
            ffrt::this_task::yield();
            }, {}, {});
        ffrt::wait();
        }, {}, {});

    ffrt::submit([&]() {
        ffrt::this_task::sleep_for(setTime);
        ffrt::this_task::yield();
        }, {}, {});

    ffrt::wait();
    return 0;
}
```


## 细粒度任务伙伴

### job_partner_attr

#### 声明

```cpp
struct job_partner_attr;
```

#### 描述

`job_partner`创建时使用的属性设置，包括QoS、Worker控制等设置。

#### 方法

##### set qos

```cpp
job_partner_attr& job_partner_attr::qos(qos q);
```

参数

- `q`：用户指定的QoS等级。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置`job_partner`执行时所使用的线程的的QoS等级。

##### get qos

```cpp
int job_partner_attr::qos() const;
```

返回值

- 返回当前QoS等级。

描述

- 获取当前属性中设置的QoS等级。

##### set max_parallelism

```cpp
job_partner_attr& job_partner_attr::max_parallelism(uint64_t v);
```

参数

- `v`：用户指定的最大Worker数。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置最大Worker数，以控制最多使用多少个Worker线程与master线程协同工作。

##### get max_parallelism

```cpp
uint64_t job_partner_attr::max_parallelism() const;
```

返回值

- 返回最大Worker数。

描述

- 获取当前属性中设置的最大Worker数。

##### set ratio

```cpp
job_partner_attr& job_partner_attr::ratio(uint64_t v);
```

参数

- `v`：用户指定的ratio参数。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置ratio参数，用于控制Worker数量。ratio表示任务数和Worker数的比例。

##### get ratio

```cpp
uint64_t job_partner_attr::ratio() const;
```

返回值

- 返回用户指定的ratio参数。

描述

- 获取当前属性中设置的ratio参数。

##### set threshold

```cpp
job_partner_attr& job_partner_attr::threshold(uint64_t v);
```

参数

- `v`：用户指定的threshold参数。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置threshold参数，用于控制Worker数量。threshold表示任务堆积到指定数量之后才会启动Worker，用于任务粒度非常小时避免Worker被频繁唤醒。

##### get threshold

```cpp
uint64_t job_partner_attr::threshold() const;
```

返回值

- 返回用户指定的threshold参数。

描述

- 获取当前属性中设置的threshold参数。

##### set busy

```cpp
job_partner_attr& job_partner_attr::busy(uint64_t us);
```

参数

- `us`：繁忙重试的时长，单位是微秒。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置Worker退出前的繁忙重试时间，在某些平台上会优化为低功耗模式，用于避免Worker频繁创建和退出。

##### get busy

```cpp
uint64_t job_partner_attr::busy() const;
```

返回值

- 返回Worker退出前的繁忙重试时间。

描述

- 获取当前属性中设置的Worker退出前的繁忙重试时间。

### job_partner

#### 声明

```cpp
template <uint64_t UsageId = 0>
struct job_partner;
```

#### 描述

`job_partner`实例，基于该类的方法可以实现partner线程与master线程协同完成计算。

UsageId表示`job_partner`实例的不同用途。

#### 方法

##### get_main_partner

```cpp
static auto& job_partner::get_main_partner();
static auto& job_partner::get_main_partner(const ffrt::job_partner_attr& attr);
```

参数

- `attr`：用户指定的`job_partner`属性。

返回值

- 返回主线程的伙伴。

描述

- 两个方法都为`job_partner`的静态方法，用于获取主线程伙伴（即`job_partner`实例）;

##### get_partner_of_this_thread

```cpp
static auto& job_partner::get_partner_of_this_thread();
static auto& job_partner::get_partner_of_this_thread(const ffrt::job_partner_attr& attr);
```

参数

- `attr`：用户指定的`job_partner`属性。

返回值

- 返回当前线程的伙伴。

描述

- 该方法为`job_partner`的静态方法，用于获取当前线程伙伴（即`job_partner`实例）。
- 同一调用线程和同一`UsageId`唯一对应一个`job_partner`实例，即：
  - 同一线程不同`UsageId`的`job_partner`调用该方法返回的`job_partner`实例是不相干的；
  - 不同线程上同一`UsageId`的`job_partner`调用该方法返回的`job_partner`实例也是不相干的。

##### submit suspendable job

```cpp
template <bool Boost = true>
int job_partner::submit(std::function<void()>&& suspendable_job, void* stack, size_t stack_size, void (*on_done)(void*) = nullptr);
```

参数

- `Boost`：可选参数，是否触发Worker按水线自动扩容。
- `suspendable_job`：任务闭包。
- `stack`：任务执行的栈空间起始地址。
- `stack_size`：任务执行的栈的大小，和stack参数匹配。
- `on_done`：任务执行完成之后回调函数地址。

返回值

- 提交成功时返回`ffrt_success`，否则返回`ffrt_error`，通常原因为`stack`为空或`stack_size`不满足最小栈空间（不同平台存在差异）限制，建议设置4KB以上的栈空间。

描述

- 提交一个可被暂停的任务，当在`suspendable_job`任务闭包中调用`submit_to_master`时，会暂停当前任务，并将`submit_to_master`入参中的闭包发回`job_partner`对应的master线程执行，直到master线程执行完闭包之后才恢复当前任务的执行。

##### submit non_suspendable job

```cpp
template <bool Boost = true>
void job_partner::submit(std::function<void()>&& non_suspendable_job);
```

参数

- `Boost`：可选参数，是否触发Worker按水线自动扩容。
- `job`：任务闭包。

描述

- 提交一个不可暂停的普通任务，即在`non_suspendable_job`任务闭包中不应该调用`submit_to_master`，若强行调用`submit_to_master`，并不会暂停当前任务的执行，而是直接执行`submit_to_master`入参中的闭包。

##### submit_p

```cpp
template <bool Boost = true, class Function>
auto job_partner::submit_p(Function&& non_suspendable_job);
```

参数

- `Boost`：可选参数，是否触发Worker按水线自动扩容。
- `Function`：`non_suspendable_job`任务类型。
- `job`：任务闭包。

返回值

- 返回执行结果的`job_promise`，通过`job_promise`可获取执行函数闭包的返回值。

描述

- 提交一个不可暂停的普通任务，并返回`promise`，用户可以通过`promise`获取任务执行结果，如果任务还未执行，则立即执行。

##### submit_to_master

```cpp
static void job_partner::submit_to_master(std::function<void()>&& job);
```

参数

- `job`：任务闭包。

描述

- 该方法为静态方法，在不同的线程上执行该方法效果不同。
- 若在partner线程上执行该方法，则会暂停当前正在执行的任务，并提交submit_to_master入参中的job任务发到对应的master线程执行，直到master线程执行完该闭包之后才恢复当前任务的执行。
- 在`suspendable job`内部，且在partner线程上执行调用该函数时才会将将job发到master线程执行，否则会在`submit_to_master`函数内部直接执行job闭包。

##### wait

```cpp
template<bool HelpPartner = true, uint64_t BusyWaitUS = 100>
int job_partner::wait();
```

参数

- `HelpPartner`：当前等待线程是否帮助partner线程消费队列任务，从时延角度，设置为true会更优。
- `BusyWaitUS`：如果`HelpPartner`设置为true，该参数无效，如果`HelpPartner`为false，当前等待线程在等待`BusyWaitUS`之后会进入睡眠，直到任务完成才被唤醒。

返回值

- 等待成功时返回0，否则返回非0，通常原因为在非master线程上执行该函数。

描述

- 在master线程上调用该接口时，同步等待`job_partner`历史提交过的所有任务执行完成，并返回0。

##### this_thread_is_master

```cpp
static bool job_partner::this_thread_is_master();
```

返回值

- 当前线程是否为`this job_partner`的是master线程。

描述

- 该方法为静态方法，判断当前线程是否为`this job_partner`的是master线程。

##### flush

```cpp
void job_partner::flush();
```

描述

- 用于在`job_partner`中的threshold为非0时，调用该函数触发Worker以执行尚未开始执行的任务，在threshold为0时，无需调用该接口。

##### try_steal_one_job

```cpp
bool try_steal_one_job();
```

返回值

- 处理任务成功时返回true，否则返回false，通常原因为队列中没有任务。

描述

- 尝试从队列中取一个任务处理，当任务队列为空时，会处理失败。
  
##### reset_queue

```cpp
void job_partner::reset_queue(uint64_t partner_queue_depth = 1024, uint64_t master_queue_depth = 1024)
```

描述

- 重新设置job_partner内部的队列深度，用于极致调优。
- 注意：这个接口只能在队列为空且Worker不在处理任务的时候调用，比如在submit之前或者wait之后。

#### 样例

````cpp
#include <iostream>
#include <array>
#include <atomic>
#include "ffrt/ffrt.h"

int main()
{
    const int job_num = 100;
    constexpr uint64_t stack_size = 16 * 1024;
    std::array<char, stack_size> stack[job_num];
    std::atomic<int> a = 0;
    auto partner = ffrt::job_partner<>::get_partner_of_this_thread();
    for (int j = 0; j < job_num; j++) {
        partner->submit([&a] {
            a++;
            for (int i = 0; i < 100; i++) {
                ffrt::job_partner<>::submit_to_master([&a] {
                    a++;
                });
            }
        }, &stack[j], stack_size);
    }
    partner->wait();
    std::cout << "a = " << a.load() << std::endl; // expect a = 10100
    return 0;
}
````

## 细粒度任务队列

### job_ring_attr

#### 声明

```cpp
struct job_ring_attr;
```

#### 描述

`job_ring`创建时使用的属性设置，包括QoS、Worker控制等设置。

#### 方法

##### set qos

```cpp
job_ring_attr& job_ring_attr::qos(qos q);
```

参数

- `q`：用户指定的QoS等级。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置`job_ring`执行时所使用的线程的的QoS等级。

##### get qos

```cpp
int job_ring_attr::qos() const;
```

返回值

- 返回当前QoS等级。

描述

- 获取当前属性中设置的QoS等级。

##### set threshold

```cpp
job_ring_attr& job_ring_attr::threshold(uint64_t v);
```

参数

- `v`：用户指定的threshold参数。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置threshold参数，用于控制Worker数量。threshold表示任务堆积到指定数量之后才会启动Worker，用于任务粒度非常小时避免Worker被频繁唤醒。

##### get threshold

```cpp
uint64_t job_ring_attr::threshold() const;
```

返回值

- 返回用户指定的threshold参数。

描述

- 获取当前属性中设置的threshold参数。

##### set busy

```cpp
job_ring_attr& job_ring_attr::busy(uint64_t us);
```

参数

- `us`：繁忙重试的时长，单位是微秒。

返回值

- 返回当前对象以支持链式调用。

描述

- 设置Worker退出前的繁忙重试时间，在某些平台上会优化为低功耗模式，用于避免Worker频繁创建和退出。

##### get busy

```cpp
uint64_t job_ring_attr::busy() const;
```

返回值

- 返回Worker退出前的繁忙重试时间。

描述

- 获取当前属性中设置的Worker退出前的繁忙重试时间。

### job_ring

#### 声明

```cpp
template <bool MultiProducer = true>
struct job_ring;
```

#### 描述

`job_ring`实例，基于该类的方法可以实现任务的生产和消费。

MultiProducer表示`job_ring`实例是否支持多生产者。

#### 方法

##### make

```cpp
static auto make(const job_ring_attr& attr = {}, uint64_t depth = 1024);
```

参数

- `attr`：用户指定的`job_ring`属性。
- `depth`：队列深度，默认1024

返回值

- 返回`job_ring`实例。

描述

- 该方法为`job_ring`的静态方法，用于获取`job_ring`实例。

##### submit

```cpp
void job_ring::submit(std::function<void()>&& job);
```

参数

- `job`：任务闭包。

描述

- 提交一个任务到`job_ring`中，如果目前没有线程执行任务，且任务数达到`threshold`数量，则会启动线程执行任务。

##### flush

```cpp
void job_ring::flush();
```

描述

- 启动线程执行任务，并且保证只有一个线程在执行任务。

##### wait

```cpp
template<bool HelpWorker = true, uint64_t BusyWaitUS = 100>
void job_ring::wait();
```

参数

- `HelpWorker`：当前等待线程是否帮助ffrt线程消费队列任务，但是会保证只有一个线程在消费队列，从时延角度，设置为true会更优。
- `BusyWaitUS`：当前等待线程在等待`BusyWaitUS`之后会进入睡眠，直到任务完成才被唤醒。

描述

- 在调用该接口时，会同步等待`job_ring`历史提交过的所有任务执行完成。

##### commit_times_for_profiling

```cpp
uint64_t& commit_times_for_profiling();
```

描述

- 此接口主要用来分析性能，查看ffrt线程启动次数。

#### 样例

````cpp
#include <iostream>
#include <array>
#include <atomic>
#include "ffrt/ffrt.h"

int main()
{
    auto q = ffrt::job_ring<false>::make(ffrt::job_ring_attr().busy(20).threshold(15));
    for (int l = 0; l < 10000; l++) {
        int c = 0;
        for (int i = 0; i < 20; i++) {
            q->submit([i, &c] {
                usleep(500);
                std::cout << "c = " << c << std::endl;  // expect c = i
                c++;
            });
        }
        q->wait();
        std::cout << "c = " << c << std::endl;  // expect c = 20
    }
    return 0;
}
````

## 细粒度任务异步返回值

### job_promise

#### 声明

```cpp
struct job_promise;
```

#### 描述

`job_promise`是任务的异步返回值，通过该类可以获取异步任务的返回值，或者在当前线程执行与之对应的任务。

#### 方法

##### is_ready

```cpp
bool is_ready() const;
```

返回值

- 返回`true`表示该任务已经完成，`false`表示该任务未执行完成。

描述

- 该方法用于判断任务完成与否。

##### get

```cpp
template <bool TryRun = true, uint64_t BusyWaitUS = 100>
decltype(auto) get(const std::function<bool()>& on_wait = nullptr);
```

参数

- `TryRun`：调用get的线程是否尝试执行`job_promise`依赖任务。设置为true时，如果任务尚未开始执行则尝试执行，否则会等待其他线程执行；从时延角度，设置为true会更优。
- `BusyWaitUS`：调用get的线程在如果`TryRun`为`false`或没有抢到任务的执行权限，则会在`BusyWaitUS`内busy查询`job_promise`是否处于ready。若为ready则返回异步任务的返回值，反之进入睡眠，直到任务完成才被唤醒。
- `on_wait`：等待promise闭包对应执行的任务闭包。

返回值

- 返回 `job_promise`对应的异步任务的返回值。

描述

- 通过调用get可以获得`job_promise`e对应的异步任务的返回值。
- `job_promise`会保证与之对应的任务闭包任何情况下只会被执行一次。
- get接口支持`TryRun`、`BusyWait`和`on_wait`参数用于极致调优，调用get时有以下情况：
  - 初始阶段：如果任务已经ready，则直接返回异步任务的返回值，否则进入下一阶段;
  - TryRun阶段：若`TryRun`为true，则尝试获取`job_promise`对应的异步任务的执行权限。如果成功获取执行权限，则执行该任务并返回，否则进入下一阶段;
  - on_wait阶段：如果`on_wait`为空则进入下一阶段，否则执行`on_wait`回调，之后检查`job_promise`是否ready。如果任务已经ready，则返回；如果任务尚未ready且`on_wait`回调返回为`true`，则继续执行`on_wait`；如果`on_wait`返回`false`则进入下一阶段;
  - BusyWaitUS阶段：如果`BusyWaitUS`不为0，则会在`BusyWaitUS`内busy查询`job_promise`是否处于ready，如果任务ready，则返回，否则进入下一阶段;
  - 睡眠等待阶段：线程睡眠，等待任务完成之后唤醒，返回。

##### execute

```cpp
bool try_execute();
```

返回值

- 返回`true`表示成功执行该任务，`false`表示该任务已经执行完成或者正在执行中，无法再次执行。

描述

- 如果`job_promise`对应的异步任务还未执行，则尝试获取执行权限，如果成功获取执行权限则直接执行并返回`true`,否则返回`false`。

#### 样例

````cpp
#include <iostream>
#include <vector>
#include "ffrt/ffrt.h"

int main()
{
    auto job_partner = ffrt::job_partner<>::get_main_partner(ffrt::job_partner_attr().max_parallelism(1));

    std::vector<ffrt::job_promise<int>> vp;
    for (int i = 0; i < 100; i++) {
        vp.emplace_back(job_partner->submit_p([i] {
            usleep(rand() % 100);
            static int a = 0;
            return a++;
        }));
    }

    for (int i = 0; i < 100; i++) {
        auto a = vp[i].get<true>([&job_partner] {
            // 如果任务正在执行则会窃取partner后面的任务开始执行
            return job_partner->try_steal_one_job();
        });
        // i为提交序，一定是保序的；a为执行序，可能会乱序(因为等待时的任务窃取)
        std::cout << "i = " << i << ", a = " << a << std::endl;
    }
    return 0;
}
````
