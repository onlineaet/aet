# AET 编译器实战：如何让一个类方法直接成为 GPU Kernel？

在上一篇文章《AET 编译器正式发布：面向异构计算与 AI 时代的新一代系统级编程语言》中，我向大家介绍了 AET 的底层架构设计。作为一门系统级异构编程语言，AET 的核心愿景是实现：**“一次编写，多芯运行”**。

文章发布后，我收到了很多底层技术爱好者的私信。大家最关心的问题无非是：*“AET 的异构计算到底长什么样？作为一个开发者，我该怎么用它写一个真正跑在 GPU 上的程序？”*

今天，我就用程序员最熟悉的“Hello World”方式，带大家亲手写一个运行在 GPU 上的核函数（Kernel），看看 AET 是如何将现代面向对象模型与 GPU 后端代码生成完美连接起来的。

---

## 一、 传统异构开发的“撕裂感”

在传统的 AI 基础设施开发中，程序员的体验往往是“撕裂”的。如果你想写一个高性能的深度学习算子（比如大模型的 Attention 或者是 CNN 的激活层），你通常需要：
1. 用 C++ 编写主控逻辑和主机端（Host）内存管理。
2. 用 CUDA C 编写专门的 `__global__` 核函数。
3. 在代码中手动处理 `cudaMalloc`、`cudaMemcpy`，并使用 `<<<grid, block>>>` 语法将数据在 CPU 和 GPU 之间来回搬运。

这种库级别的拼接，不仅让代码结构变得臃肿（例如经典的 C++ 无法原生优雅地在显存里映射一个包含复杂行为的“类对象”），也增加了编译器进行跨平台统一优化的难度。

**AET 改变了这种游戏规则：它不把异构计算当作外部库，而是将其作为语言和编译器内核的一部分。**

---

## 二、 AET 核心实战：写一个运行在 GPU 上的核函数

在 AET 中，我们引入了独创的 `class$` 和 `impl$`等关键字来实现现代软件工程的组织能力，同时原生支持 GPU 核函数修饰符 `__global__` 和设备函数修饰符 `__device__`。

为了验证编译器的生成能力，我们来看一段真正的 AET 验证程序。在 AET 中，一个 GPU Kernel 可以直接作为类的方法：

```aet
#include <stdio.h>

// 1. 使用 class$ 声明类与 GPU 核函数
class$ HelloGPU
{
    __global__ void hello();
};

// 2. 使用 impl$ 实现类的方法
impl$ HelloGPU
{
    __global__ void hello(){
        printf("hello world from GPU\n");
        int id = threadIdx.x; // 支持 GPU 线程内置变量
        printf("thread id = %d\n", id);
    }
};

int main()
{
    // 3. 实例化对象并直接以类方法的形式启动 GPU 内核
    HelloGPU *gpu = new$ HelloGPU();
    gpu->hello<<<1, 2>>>();
    
    // 硬件层同步
    MtcsSystem.synchronize();
    return 0;
}
```

这个简单程序经过 AET 编译流程后，会生成 NVIDIA PTX 代码，部分核心切片如下：

```ptx
...
.visible .entry _Z8HelloGPU5helloEPN8HelloGPUE (.param.u64 %in_ar0)
{
	.reg.u64 %ar0;
	ld.param.u64 %ar0, [%in_ar0];
	.local .align 8 
...
```

**运行结果：**
```text
hello world from GPU
hello world from GPU
thread id = 0
thread id = 1
```

---

## 三、 AET 中对象如何跨越 GPU 执行空间？

在上面的简单示例中，我们展示了一个类方法如何直接编译为 GPU Kernel。但这只是异构面向对象开发的第一步。对于真正复杂的工业级程序（例如 AI 网络层、计算图或高性能算子），仅仅生成一个纯函数的 Kernel 是远远不够的。

我们要面对一个所有异构语言都无法回避的核心问题：**一个面向对象的实例，究竟该如何自然地跨越 CPU 和 GPU 两个完全隔离的执行空间？**

### 1. 传统异构编程的“数据与对象分离”痛点
在传统的 CUDA C++ 编程中，CPU 侧的“对象状态”和 GPU 侧的“数据指针”通常是强行割裂的。想象一个标准的 AI 网络层：

```cpp
class Layer {
    float *weight;    // 存储在显存中
    int batch_size;   // 存储在内存中
    void forward();
};
```

这个对象默认存在 CPU 内存中，但其核心计算逻辑 `forward()` 却要跑在 GPU 上。为了让它跑起来，开发者必须手写大量的宿主（Host）与设备（Device）同步逻辑，痛苦地在 CPU 对象和原始 GPU 数据指针之间编织数据纽带。这种做法使得面向对象的封装性、继承性在异构屏障面前变得支离破碎。

**AET 编译器从根本上推翻了这种模式。我们不要求开发者去适应硬件，而是通过编译器和运行时的智能推导，让硬件隐式适配代码。**

### 2. 编译器分析 + AET 运行时（Runtime）的分配双轨制
在 AET 语言中，当你写下一行最普通的实例化代码时：

```aet
Object *a = new$ Object();
```

AET 编译器在编译过程中，会通过对象分析和依赖分析判断该对象是否参与 MTCS （矩阵芯片系统 ）异构执行。此时，编译器会在底层自动切换两种完全不同的分配方案：

*   **与异构无关对象（纯 CPU 堆分配）：** 如果该类只包含普通逻辑，不涉及任何 `__global__` 或 `__device__` 调用，AET 会让其退化为最高效的传统 CPU 堆内存分配（Heap Allocation）。
*   **异构相关对象（设备可访问内存分配）：** 一旦检测到该对象属于异构计算上下文，AET 运行时会自动接管。在目前的 NVIDIA CUDA 后端上，AET 会在底层自动将其映射为**统一内存（Unified Memory）**。这使得 CPU 和 GPU 可以通过统一内存访问同一个对象状态。

### 3. 栈对象的生命周期管理与运行时复制机制
除了堆内存指针，程序员同样喜欢在栈上创建对象：

```aet
Object a = new$ Object();
```

在传统的异构编程中，GPU 是绝对无法直接访问 CPU 线程栈帧内部的数据的。为了让这种现代编程习惯在 GPU 上得以延续，当这个栈对象作为上下文进入 GPU Kernel 执行空间时，AET 的运行时系统会在内核启动（Kernel Launch）的瞬间，**在硬件设备端自动完成对象的深度镜像复制（Deep Copy）**。它将 CPU 栈上的成员变量布局复制映射到设备端内存中。

得益于这种精妙的设计，在 AET 的 GPU Kernel 内部，开发者可以使用极其自然的现代语法：

```aet
class$ HelloGPU {
    int value;
    __global__ void hello();
};

impl$ HelloGPU {
    __global__ void hello() {
        // 在 GPU 内部，self 依然完美代表当前对象！
        printf("Value from GPU context: %d\n", self->value);
    }
};
```

你不再需要手动将 `host_object` 解包、再用 `cudaMemcpy` 传给 `device_data`、最后作为一堆散装参数传给 Kernel。在 AET里，self 引用在 GPU 线程执行环境中仍然保持面向对象访问方式。

---

## 四、 严谨的边界与未来的探索

当然，CPU 和 GPU 毕竟属于不同的执行空间。因此目前 AET 对对象方法的访问也有着明确的显式边界：

在 `__global__` 和 `__device__` 函数中，开发者可以完美访问：
*   **对象的成员变量**
*   **被修饰为 GPU 可执行的方法（即其他核函数或设备函数）**

但**不能**直接调用普通的 CPU 方法。

例如：

```aet
class$ Test
{
    void cpuFunction(); // 普通 CPU 方法，生成 x86/ARM 指令

    __device__ void gpuFunction()
    {
        cpuFunction();   // 🛑 编译错误：不允许直接调用！
    }
};
```

**这一限制的原因不是语言层面的刻意阉割，而是底层物理执行环境决定的铁律：**普通的 CPU 方法并不存在于 GPU 的代码执行空间中，硬件无法跨越指令集去解析或执行异构代码段。

客观来看，受限于当前芯片硬件架构的天然割裂，在核函数或设备函数中访问复杂的类模型，在工程实践中仍然存在进一步完善空间。如何在这个“硬件禁区”里戴着镣铐跳舞，构建更自然的异构对象模型，是 AET 需要攻克的技术难点，这包括：
*   更低开销的跨硬件对象生命周期管理；
*   编译期全自动的 CPU/GPU 状态同步；

这些不仅是 AET 正在探索的未知领域，也是未来整个 AI 异构编程语言想要推动基础设施开发方式演进，必须给出的终极答案。

---

*   **AET 编译器项目地址：** [GitHub - onlineaet/aet](https://github.com/onlineaet/aet)
*   **AET-CNN 验证框架：** [GitHub - onlineaet/aet-cnn](https://github.com/onlineaet/aet-cnn)

