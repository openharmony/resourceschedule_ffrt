## 有栈协程

### ffrt_fiber_t

#### 声明

```c
typedef struct {
    uintptr_t storage[ffrt_fiber_storage_size];
} ffrt_fiber_t;
```

#### 描述

提供协程切换上下文存储和读取功能。

#### 方法

##### ffrt_fiber_init

声明

```c
FFRT_C_API int ffrt_fiber_init(ffrt_fiber_t* fiber, void(*func)(void*), void* arg, void* stack, int stack_size);
```

参数

- `fiber`：fiber指针。
- `func`：fiber运行时当前方法指针。
- `arg`：fiber运行时当前方法入参。
- `stack`：fiber运行时当前方法地址。
- `stack_size`：fiber运行时当前方法内存大小。

返回值

- `int`初始化成功与否 0：成功。

描述

- 初始化fiber。

##### ffrt_fiber_switch

声明

```c
FFRT_C_API void ffrt_fiber_switch(ffrt_fiber_t* from, ffrt_fiber_t* to);
```

参数

- `from`：当前协程状态。
- `to`：之前协程状态。

描述

- 切换协程上下文内容。
