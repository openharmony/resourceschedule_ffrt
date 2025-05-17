# Function Flow Runtime开发样例(Kotlin)

本文档旨在介绍如何使用Kotlin语言以及Kotlin与Native层互操作的技术来为FFRT编写包装函数，并提供一些使用这些包装函数的示例。

## Kotlin语言简介

Kotlin是一种现代的、静态类型的编程语言，由JetBrains开发。它设计目标是成为“更好的Java”，但也可以编译成JavaScript或Native代码。Kotlin具有以下主要特点：

- **简洁性：** Kotlin提供了简洁的语法，可以减少样板代码。
- **空安全：** Kotlin的类型系统可以帮助避免空指针异常。
- **互操作性：** Kotlin可以与Java代码无缝互操作。
- **协程：** Kotlin支持协程，这是一种轻量级的并发机制。
- **多平台：** Kotlin可以编译成JVM字节码、JavaScript或Native代码。

## Kotlin/Native与Native层互操作

Kotlin/Native允许Kotlin代码直接与Native层C/C++代码进行交互。这是通过`cinterop`工具实现的，该工具可以生成Kotlin代码来调用C/C++库。

**互操作原理：**

1. **定义C接口：** 首先，你需要有一个C的头文件，其中声明了你想要调用的C函数和数据结构。
2. **`cinterop`工具：** 使用Kotlin/Native提供的`cinterop`工具，根据C头文件生成Kotlin代码。这个生成的代码包含了C函数的Kotlin包装器和C数据结构的Kotlin表示。
3. **Kotlin代码调用：** Kotlin代码就可以像调用普通的Kotlin函数一样调用生成的包装器函数，从而间接地调用C函数。

**使用`cinterop`的步骤：**

1. **配置`build.gradle.kts`：** 在Gradle构建文件中配置 `cinterop`。
2. **创建`.def`文件：** 创建一个`.def`文件，其中指定要包含的C头文件、库和其他编译/链接选项。
3. **编写Kotlin代码：** 在Kotlin代码中导入并使用`cinterop`生成的API。

更多的Kotlin与C语言互操作性的指导可以参阅官方文档：[Interoperability with C](https://kotlinlang.org/docs/native-c-interop.html)

## 为什么要为FFRT编写Kotlin包装函数

FFRT主要提供C和C++的接口。虽然可以直接在Kotlin/Native中使用这些接口，但这样做通常比较繁琐，且容易出错。为FFRT编写Kotlin包装函数存在如下收益：

- **简化API使用：** Kotlin包装函数可以提供更简洁、更符合Kotlin语言习惯的API，隐藏底层的C/C++细节。
- **提高类型安全性：** Kotlin的强类型系统可以帮助避免因类型错误导致的错误。
- **资源管理：** Kotlin包装函数可以更好地管理FFRT资源的生命周期，例如内存分配和释放。
- **提供更高级的抽象：** Kotlin包装函数可以提供更高级的抽象，例如使用Kotlin的集合类来代替C数组。
- **提高开发效率：** 使用Kotlin包装函数可以提高开发效率，减少代码量。

## FFRT的Kotlin封装 (Task.kt和Queue.kt)

以下是FFRT的`Task`和`Queue`接口的一种Kotlin封装实现，**请注意，这只是一个示例，用于抛砖引玉，可能存在其他更好的封装方式。**

包装库的文件目录为：

```plain
ffrt-wrapper/
├── build.gradle.kts
├── settings.gradle.kts  (需要包含 ':ffrt-wrapper')
├── src/
│   ├── nativeInterop/
│   │   └── cinterop/
│   │       └── ffrt.def       <-- cinterop 定义文件
│   └── nativeMain/
│       └── kotlin/
│           ├── Queue.kt      <-- 队列封装
│           └── Task.kt       <-- 任务封装
```

其中`build.gradle.kts`配置文件如下所示：

```kotlin
// build.gradle.kts
plugins {
    kotlin("multiplatform") version "1.9.22" // 修改成项目中实际使用的版本
}

kotlin {
    linuxX64("native") {
        binaries {
            sharedLib {
                baseName = "ffrt-wrapper"
            }
        }
        compilations.getByName("main") {
            cinterops {
                val ffrt by creating {
                    defFile = file("src/nativeInterop/cinterop/ffrt.def")
                    packageName = "com.huawei.ffrt"
                }
            }
        }
    }

    sourceSets {
        val nativeMain by getting {
            dependencies {
                // 依赖配置（如果有）
            }
            kotlin.srcDirs("src/nativeMain/kotlin")
        }
    }
}
```

其中`ffrt.def`配置文件如下所示：

```ini
// ffrt.def
headers = path/to/queue.h, path/to/task.h
package = com.huawei.ffrt
compilerOpts = -I/path/to/ffrt/include
linkerOpts = -L/path/to/ffrt/lib -lffrt
```

> **说明：**
>
> - `Task.kt`和`Queue.kt`中的封装函数仅适配了不带参数的任务闭包，如果业务需要带参数的任务闭包可以修改`createFunctionWrapper`函数。
> - 用户也可以在现有封装的基础上，再进一步封装更高级、更Kotlin风格的DSL。

### Task.kt

`Task.kt`文件封装了FFRT中与任务相关的接口。

- **`TaskAttr`类：** 封装了`ffrt_task_attr_t`结构体，用于设置任务的属性（例如，任务名称、QoS）。实现了`Closeable`接口来管理资源。
- **`TaskHandle`类：** 封装了`ffrt_task_handle_t`，用于表示一个任务句柄。实现了`Closeable`接口来管理任务句柄的生命周期。
- **`Dependence`类：** 封装了`ffrt_dependence_t`，用于表示任务之间的依赖关系。
- **`Task`object：** 提供了静态函数来提交任务 (`submit`、`submitWithHandle`)、等待任务完成 (`wait`、`waitDeps`) 等。

封装代码如下所示：

```kotlin
import kotlinx.cinterop.*
import kotlin.io.Closeable
import com.huawei.ffrt.*

// 封装 ffrt_queue_priority_t
enum class QueuePriority(val value: Int) {
    IMMEDIATE(ffrt_queue_priority_t.ffrt_queue_priority_immediate),
    HIGH(ffrt_queue_priority_t.ffrt_queue_priority_high),
    LOW(ffrt_queue_priority_t.ffrt_queue_priority_low),
    IDLE(ffrt_queue_priority_t.ffrt_queue_priority_idle);

    override fun toString(): String = name

    companion object {
        fun fromInt(value: Int): QueuePriority? = values().find { it.value == value }
    }
}

// 封装 ffrt_task_attr_t
class TaskAttr : Closeable {
    private val struct = nativeHeap.alloc<ffrt_task_attr_t>()
    internal val nativeAttr: CPointer<ffrt_task_attr_t> = struct.ptr
    private var namePtr: CPointer<ByteVar>? = null

    init { ffrt_task_attr_init(nativeAttr) }

    constructor(name: String? = null) : this() {
        this.name = name
    }

    var name: String?
        get() = ffrt_task_attr_get_name(nativeAttr)?.toKString()
        set(value) {
            namePtr?.let {
                nativeHeap.free(it)
                namePtr = null
            }
            namePtr = value?.cstr?.getPointer(nativeHeap)
            ffrt_task_attr_set_name(nativeAttr, namePtr)
        }

    fun name(name: String?): TaskAttr {
        this.name = name
        return this
    }

    var qos: Int
        get() = ffrt_task_attr_get_qos(nativeAttr)
        set(value) { ffrt_task_attr_set_qos(nativeAttr, value) }

    fun qos(qos: Int): TaskAttr {
        this.qos = qos
        return this
    }

    var timeout: Long
        get() = ffrt_task_attr_get_timeout(nativeAttr).toLong()
        set(value) { ffrt_task_attr_set_timeout(nativeAttr, value.convert()) }

    fun timeout(timeout: Long): TaskAttr {
        this.timeout = timeout
        return this
    }

    var delay: Long
        get() = ffrt_task_attr_get_delay(nativeAttr).toLong()
        set(value) { ffrt_task_attr_set_delay(nativeAttr, value.convert()) }

    fun delay(delay: Long): TaskAttr {
        this.delay = delay
        return this
    }

    var queuePriority: QueuePriority
        get() = QueuePriority.values().first { it.value == ffrt_task_attr_get_queue_priority(nativeAttr).value }
        set(value) {
            ffrt_task_attr_set_queue_priority(nativeAttr, value.value)
        }

    fun queuePriority(priority: QueuePriority): TaskAttr {
        this.queuePriority = priority
        return this
    }

    override fun close() {
        ffrt_task_attr_destroy(nativeAttr)
        namePtr?.let {
            nativeHeap.free(it)
            namePtr = null
        }
        nativeHeap.free(struct)
    }
}

// 封装 ffrt_task_handle_t
class TaskHandle(internal val nativeHandle: ffrt_task_handle_t) : Closeable {
    override fun close() { ffrt_task_handle_destroy(nativeHandle) }
}

// 封装 ffrt_dependence_t
class Dependence : Closeable {
    internal val nativeDependence: ffrt_dependence_t
    private var isTaskDependence: Boolean = false

    // 数据依赖
    constructor(data: COpaquePointer?) {
        nativeDependence = ffrt_dependence_t().apply {
            type = ffrt_dependence_type_t.ffrt_dependence_data
            ptr = data
        }
    }

    // 任务依赖
    constructor(taskHandle: TaskHandle) {
        nativeDependence = ffrt_dependence_t().apply {
            type = ffrt_dependence_type_t.ffrt_dependence_task
            ptr = taskHandle.nativeHandle
        }
        ffrt_task_handle_inc_ref(taskHandle.nativeHandle)
        isTaskDependence = true
    }

    override fun close() {
        if (isTaskDependence && nativeDependence.ptr != null) {
            ffrt_task_handle_dec_ref(nativeDependence.ptr.reinterpret())
        }
    }
}

fun createFunctionWrapper(
    func: () -> Unit,
    kind: ffrt_function_kind_t
): CPointer<ffrt_function_header_t> {
    val stableRef = StableRef.create(func)
    val functionStorage = ffrt_alloc_auto_managed_function_storage_base(kind)
        ?: throw Error("Failed to allocate function storage")

    /*
        functionStorage:
        +-----------------------------+ <-- arg
        | ffrt_function_header_t      | <-- header fields (exec, destroy, ...)
        |   exec = execFunction       |
        |   destroy = destroyFunction |
        |   reserve[2] = 0            |
        +-----------------------------+
        | StableRef<() -> Unit>       | <-- closure ptr (offset = sizeof(header))
        +-----------------------------+
    */
    val header = functionStorage.reinterpret<ffrt_function_header_t>()
    header.pointed.exec = staticCFunction { arg ->
        extractClosurePtr(arg).asStableRef<() -> Unit>().get().invoke()
    }
    header.pointed.destroy = staticCFunction { arg ->
        extractClosurePtr(arg).asStableRef<() -> Unit>().dispose()
    }
    header.pointed.reserve[0] = 0uL
    header.pointed.reserve[1] = 0uL

    // 将 StableRef 存储在 header 后面，用于 C++ 风格封装
    val headerSize = sizeOf<ffrt_function_header_t>()
    val closureStorageOffset = functionStorage.rawValue + headerSize
    closureStorageOffset.reinterpret<ULongVar>().pointed.value = stableRef.asCPointer().rawValue.toULong()

    return header
}

private fun extractClosurePtr(arg: COpaquePointer?): COpaquePointer {
    requireNotNull(arg)
    val headerSize = sizeOf<ffrt_function_header_t>()
    val closureOffset = arg.rawValue + headerSize
    return closureOffset.toCPointer<COpaquePointerVar>()!!.pointed.value!!
}

object Task {
    // 封装 ffrt_submit_base
    fun submit(
        func: () -> Unit,
        inDeps: List<Dependence> = emptyList(),
        outDeps: List<Dependence> = emptyList(),
        attr: TaskAttr = TaskAttr()
    ) {
        memScoped {
            val inDepsNative = inDeps.toNativeDeps(this)
            val outDepsNative = outDeps.toNativeDeps(this)
            val funcWrapper = createFunctionWrapper(func, ffrt_function_kind_t.ffrt_function_kind_general)
            ffrt_submit_base(funcWrapper, inDepsNative.ptr, outDepsNative.ptr, attr.nativeAttr)
        }
        inDeps.forEach { it.close() }
        outDeps.forEach { it.close() }
        attr.close()
    }

    // 封装 ffrt_submit_h_base
    fun submitWithHandle(
        func: () -> Unit,
        inDeps: List<Dependence> = emptyList(),
        outDeps: List<Dependence> = emptyList(),
        attr: TaskAttr = TaskAttr()
    ): TaskHandle {
        return memScoped {
            val inDepsNative = inDeps.toNativeDeps(this)
            val outDepsNative = outDeps.toNativeDeps(this)
            val funcWrapper = createFunctionWrapper(func, ffrt_function_kind_t.ffrt_function_kind_general)
            val handle = ffrt_submit_h_base(funcWrapper, inDepsNative.ptr, outDepsNative.ptr, attr.nativeAttr)
            TaskHandle(handle!!)
        }.also {
            inDeps.forEach { it.close() }
            outDeps.forEach { it.close() }
            attr.close()
        }
    }

    // 封装 ffrt_wait
    fun wait() {
        ffrt_wait()
    }

    // 封装 ffrt_wait_deps
    fun waitDeps(deps: List<Dependence>) {
        memScoped {
            val nativeDeps = deps.toNativeDeps(this)
            ffrt_wait_deps(nativeDeps.ptr)
        }
        deps.forEach { it.close() }
    }

    private fun List<Dependence>.toNativeDeps(scope: MemScope): ffrt_deps_t {
        if (this.isEmpty()) { return ffrt_deps_t() }
        val nativeDeps = scope.allocArray<ffrt_dependence_t>(this.size)
        for (i in this.indices) {
            nativeDeps[i] = this[i].nativeDependence
        }
        return ffrt_deps_t().apply {
            len = this@toNativeDeps.size.convert()
            items = nativeDeps
        }
    }
}
```

### Queue.kt

`Queue.kt`文件封装了FFRT中与队列相关的接口。

- **`QueueAttr`类：** 封装了 `ffrt_queue_attr_t` 结构体，用于设置队列的属性（例如，队列类型、QoS）。实现了`Closeable`接口来管理资源。
- **`QueueType`enum class：** 封装了FFRT中定义的队列类型。
- **`Queue`类：** 封装了`ffrt_queue_t`，用于表示一个FFRT队列。提供了函数来提交任务 (`submit`、`submitWithHandle`)、取消任务 (`cancel`)、等待任务完成 (`wait`) 等。

封装代码如下所示：

```kotlin
import kotlinx.cinterop.*
import kotlin.io.Closeable
import com.huawei.ffrt.*

// 封装 ffrt_queue_type_t
enum class QueueType(val value: Int) {
    SERIAL(ffrt_queue_type_t.ffrt_queue_serial),
    CONCURRENT(ffrt_queue_type_t.ffrt_queue_concurrent);

    override fun toString(): String = name

    companion object {
        fun fromInt(value: Int): QueueType? = values().find { it.value == value }
    }
}

// 封装 ffrt_queue_attr_t
class QueueAttr : Closeable {
    private val struct = nativeHeap.alloc<ffrt_queue_attr_t>()
    internal val nativeAttr: CPointer<ffrt_queue_attr_t> = struct.ptr

    init {
        ffrt_queue_attr_init(nativeAttr)
    }

    var qos: Int
        get() = ffrt_queue_attr_get_qos(nativeAttr)
        set(value) { ffrt_queue_attr_set_qos(nativeAttr, value) }

    fun qos(qos: Int): QueueAttr {
        this.qos = qos
        return this
    }

    var maxConcurrency: Int
        get() = ffrt_queue_attr_get_max_concurrency(nativeAttr)
        set(value) { ffrt_queue_attr_set_max_concurrency(nativeAttr, value) }

    fun maxConcurrency(value: Int): QueueAttr {
        this.maxConcurrency = value
        return this
    }

    override fun close() {
        ffrt_queue_attr_destroy(nativeAttr)
        nativeHeap.free(struct)
    }
}

class Queue internal constructor(
    private val queueHandle: ffrt_queue_t,
    private val ownsHandle: Boolean = true
) : Closeable {
    constructor(type: QueueType, name: String, attr: QueueAttr = QueueAttr()) : this(
        ffrt_queue_create(type.value, name.cstr.ptr, attr.nativeAttr) ?: error("Queue creation failed")
    )

    constructor(name: String, attr: QueueAttr = QueueAttr()) : this(
        ffrt_queue_create(QueueType.SERIAL.value, name.cstr.ptr, attr.nativeAttr) ?: error("Queue creation failed")
    )

    // 封装 ffrt_queue_submit
    fun submit(func: () -> Unit, attr: TaskAttr = TaskAttr()) {
        val f = createFunctionWrapper(func, ffrt_function_kind_t.ffrt_function_kind_queue)
        ffrt_queue_submit(queueHandle, f, attr.nativeAttr)
        attr.close()
    }

    // 封装 ffrt_queue_submit_h
    fun submitWithHandle(func: () -> Unit, attr: TaskAttr = TaskAttr()): TaskHandle {
        val f = createFunctionWrapper(func, ffrt_function_kind_t.ffrt_function_kind_queue)
        val handle = ffrt_queue_submit_h(queueHandle, f, attr.nativeAttr)
        attr.close()
        return TaskHandle(handle ?: error("Queue submit_h failed"))
    }

    // 封装 ffrt_queue_cancel
    fun cancel(handle: TaskHandle): Int = ffrt_queue_cancel(handle.nativeHandle)

    // 封装 ffrt_queue_wait
    fun wait(handle: TaskHandle) = ffrt_queue_wait(handle.nativeHandle)

    override fun close() {
        if (ownsHandle) {
            ffrt_queue_destroy(queueHandle)
        }
    }

    companion object {
        fun getMainQueue(): Queue? {
            val q = ffrt_get_main_queue()
            return if (q != null) Queue(q, ownsHandle = false) else null
        }
    }
}
```

## 使用Kotlin封装的示例

以下是一些使用上面介绍的Kotlin封装来编写的FFRT示例。

### Fibonacci数列

这个示例展示了如何使用FFRT并行计算Fibonacci数列。具体用例场景可以参考：[图依赖并发(C++)](ffrt-concurrency-graph-cpp.md)。

```kotlin
import kotlinx.cinterop.*
import com.huawei.ffrt.*

data class FibArgs(val x: Int, var y: Int)

fun fibonacci(x: Int): Int {
    return if (x <= 1) {
        x
    } else {
        fibonacci(x - 1) + fibonacci(x - 2)
    }
}

fun main() {
    val result = FibArgs(5, 0)

    Task.submit {
        result.y = fibonacci(result.x)
    }

    Task.wait() // 等待任务完成

    println("Fibonacci(${result.x}) is ${result.y}")
}
```

### 任务图依赖

这个示例展示了如何使用FFRT创建和执行具有依赖关系的多个任务。具体用例场景可以参考：[图依赖并发(C++)](ffrt-concurrency-graph-cpp.md)。

```kotlin
import kotlinx.cinterop.*
import com.huawei.ffrt.*

fun main() {
    // 提交任务A
    val handleA = Task.submitWithHandle {
        println("视频解析")
    }

    // 提交任务B和C
    val depsA = listOf(Dependence(handleA))
    val handleB = Task.submitWithHandle(
        { println("视频转码") },
        inDeps = depsA
    )
    val handleC = Task.submitWithHandle(
        { println("视频生成缩略图") },
        inDeps = depsA
    )

    // 提交任务D
    val depsBC = listOf(Dependence(handleB), Dependence(handleC))
    val handleD = Task.submitWithHandle(
        { println("视频添加水印") },
        inDeps = depsBC
    )

    // 提交任务E
    val depsD = listOf(Dependence(handleD))
    Task.submit(
        { println("视频发布") },
        inDeps = depsD
    )

    // 等待所有任务完成
    Task.wait()
}
```

### 串行队列

这个示例展示了如何使用FFRT队列来实现一个简单的日志系统。具体用例场景可以参考：[串行队列(C++)](ffrt-concurrency-serial-queue-cpp.md)。

```kotlin
import kotlinx.cinterop.*
import kotlin.io.Closeable
import com.huawei.ffrt.*
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

// 封装日志系统
class Logger(filename: String) : Closeable {

    private val queue: Queue = Queue("logger_queue") // 创建 FFRT 串行队列
    private val logFile = File(filename)
    private val outputStream: FileOutputStream

    init {
        try {
            outputStream = FileOutputStream(logFile, true) // 以追加模式打开文件
            println("Log file opened: $filename")
        } catch (e: IOException) {
            throw IOException("Failed to open log file: $filename", e)
        }
    }

    fun log(message: String) {
        queue.submit {
            try {
                outputStream.write("$message\n".toByteArray())
                outputStream.flush()
            } catch (e: IOException) {
                System.err.println("Error writing to log: ${e.message}")
            }
        }
    }

    override fun close() {
        queue.close() // 销毁 FFRT 队列
        try {
            outputStream.close()
            println("Log file closed")
        } catch (e: IOException) {
            System.err.println("Error closing log file: ${e.message}")
        }
    }
}

fun main() {
    val logger = Logger("log.txt")

    // 主线程添加日志任务
    logger.log("Log message 1")
    logger.log("Log message 2")
    logger.log("Log message 3")

    // 模拟主线程继续执行其他任务
    Thread.sleep(1000) // Kotlin/JVM 的 sleep (使用 Long)

    logger.close()
}
```

### 并发队列

这个示例展示了如何使用FFRT队列来实现一个简单的银行服务系统。具体用例场景可以参考：[并发队列(C++)](ffrt-concurrency-concurrent-queue-cpp.md)。

```kotlin
import kotlinx.cinterop.*
import kotlin.io.Closeable
import com.huawei.ffrt.*

// 封装并发队列系统
class ConcurrentQueueSystem(name: String, concurrency: Int) : Closeable {

    private val queue: Queue

    init {
        val queueAttr = QueueAttr().maxConcurrency(concurrency)
        queue = Queue(QueueType.CONCURRENT, name, queueAttr)
        println("Concurrent queue system '$name' initialized with concurrency $concurrency")
    }

    // 提交任务到队列
    fun enqueue(
        taskName: String,
        qos: Int,
        delayMillis: Long = 0,
        priority: QueuePriority,
        block: () -> Unit
    ): TaskHandle {
        val taskAttr = TaskAttr()
            .name(taskName)
            .qos(qos)
            .delay(delayMillis.toInt())
            .queuePriority(priority)
        return queue.submitWithHandle(block, taskAttr)
    }

    // 取消任务
    fun cancel(taskHandle: TaskHandle): Int {
        return queue.cancel(taskHandle)
    }

    // 等待任务完成
    fun wait(taskHandle: TaskHandle) {
        queue.wait(taskHandle)
    }

    override fun close() {
        queue.close()
        println("Concurrent queue system closed")
    }
}

fun bankBusiness(customerName: String) {
    Thread.sleep(100) // 模拟耗时操作
    println("Serving customer: $customerName in thread ${Thread.currentThread().name}")
}

fun bankBusinessVIP(customerName: String) {
    Thread.sleep(50) // 模拟VIP服务更快
    println("Serving VIP customer: $customerName in thread ${Thread.currentThread().name}")
}

fun main() {
    val bankQueueSystem = ConcurrentQueueSystem("Bank", 2) // 银行有2个窗口

    val task1 = bankQueueSystem.enqueue("Customer1", QueuePriority.LOW, 0) {
        bankBusiness("Customer1")
    }

    val task2 = bankQueueSystem.enqueue("Customer2", QueuePriority.LOW, 0) {
        bankBusiness("Customer2")
    }

    val task3 = bankQueueSystem.enqueue("Customer3 VIP", QueuePriority.HIGH, 0) {
        bankBusinessVIP("Customer3 VIP")
    }

    val task4 = bankQueueSystem.enqueue("Customer4", QueuePriority.LOW, 0) {
        bankBusiness("Customer4")
    }

    val task5 = bankQueueSystem.enqueue("Customer5", QueuePriority.LOW, 0) {
        bankBusiness("Customer5")
    }

    // 模拟取消一个任务
    bankQueueSystem.cancel(task4)

    // 模拟等待任务完成
    bankQueueSystem.wait(task5)

    bankQueueSystem.close()
}
```
