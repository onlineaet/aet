## AET 编译器：关键字 super$ 如何跨越 CPU/GPU 地址空间



### 一、 简介

在软件工程的发展过程中，面向对象编程（OOP）提供了一种让复杂系统保持可理解、可扩展与可维护的语言范式。其中，`super` 作为继承体系中的重要机制，使子类能够复用和扩展父类行为，让类与类之间的派生关系清晰、可控。

然而，传统面向对象机制存在一个隐含前提：**所有对象实例与方法均运行在同一个统一的执行空间（Host 侧）**。当程序进入 CPU/GPU 异构计算时代后，该前提被打破。一个看似简单的调用：

```
super->method();
```

在异构环境中，不再仅是一次简单的控制流跳转，而演变为**跨地址空间、跨硬件执行体系的方法绑定与寻址问题**。

在 AET 语言中，`super$` 关键字被设计用于支持以下五种场景：

1. 当子类想要访问父类的同名同参数方法。
2. 想要在构造函数里访问父类的构造方法。
3. 亦或是父子类存在同名变量需要显式访问父类成员时。
4. **核函数（`__global__`）调用父类的设备函数（`__device__`）。**
5. **设备函数（`__device__`）调用父类的设备函数（`__device__`）。**

以下是一段用于验证 AET 编译器该特性的真实程序：

```
// 父类：声明并实现一个 GPU 设备函数
class$ Activation {
   __device__ float leaky(float x);
};

impl$ Activation {
   __device__ float leaky(float x) {
      return (x > 0) ? x : .1 * x;
   }
};

// 子类：继承自 Activation
class MtcsActivation extends Activation {
   __global__ void compute(float x);
};

impl$ MtcsActivation {
   __global__ void compute(float x) {
      // 核心挑战：在 GPU Kernel 中利用 super$ 调用父类 Activation 的设备方法
      float r = super$->leaky(x); 
      printf("super leaky :%f\n", r);
   }
};

int main() {
   MtcsActivation *tcs = new$ MtcsActivation();
   tcs->compute(5.0);
   return 0;
}
```

在传统 CPU 架构下，编译器利用统一的内存地址空间和成熟的对象布局规则，将继承关系转换为普通函数调用、静态绑定或虚函数表（vtable）访问。但在异构计算中，一旦允许 `super$` 在内核函数（`__global__`）或设备函数（`__device__`）内部跨空间调用父类方法，编译器就必须克服 **Host 与 Device 之间不同的内存空间、指令代码以及物理硬件特性差异**带来的编译拓扑挑战。

------

### 二、 给开发者的益处（关键技术价值）

AET 编译器通过底层架构创新实现了这一机制，为异构计算开发者带来以下开发便利：

#### 1.生产力释放，降低异构编程复杂度

在传统的 CUDA C++ 或 AI 算子开发中，若想让子类算子复用父类算子的行为，开发者必须手动将父类逻辑拆解为纯 C 风格的全局函数，并通过显式的指针、独立的参数传递，甚至手动调用 `cudaMemcpy` 进行数据搬运。这种方式不仅导致代码极其臃肿，且一旦父类接口发生改动，所有子类的内核启动（Kernel Launch）逻辑全部都要手动重写。

通过 `super$` 的语法抽象，开发者在 GPU 核函数内部仅需写下一行最符合直觉的面向对象代码：

```
float r = super$->leaky(x);
```

其背后的**符号克隆、设备函数地址反向回填、中间表示（IR）降维替换**均由编译器自动完成。对用户而言，复杂的异构底层被抽象为了高级软件工程模型。开发效率成倍提升，原本需要开发者显式维护的设备函数关系、调用路径和地址绑定，由编译器自动完成，实现了对开发者生产力的极大解放。

#### 2.让大型 AI / HPC 基础设施具备“真正的模块化能力”

当前多数 GPU 软件和高性能算子库由于硬件限制，实际上被迫退化为过程式设计，导致大量类似 

例如：

```cpp
conv_forward_gpu();

relu_forward_gpu();

pool_forward_gpu();
```

 的平铺函数堆积，极其难以维护。

然而，现实世界的高层抽象更适合采用拓扑结构：

```
Layer
 └── ConvLayer
 └── ActivationLayer
 └── PoolLayer
```

对象之间拥有继承、组合、多态等结构形式。AET 的核心价值在于建立了异构环境下的对象行为映射模型，使得：

```
Layer
   └── GPU method
```

成为可能。也就是说，GPU 不再只是一个计算加速器，而成为了整个软件对象模型中有机的一部分。

#### 3.编译器驱动的低开销抽象

很多开发者担心：

> 面向对象抽象是否会影响 GPU 性能？


AET 的设计目标不是增加一层复杂运行时，而是在编译阶段完成对象关系和设备函数之间的映射。


`super$->leaky(x)` 最终转换为设备函数地址调用。

它避免了传统对象系统中大量运行时管理逻辑。

------

### 三、 super$ 实现原理与编译管线

AET 编译器对 `super$` 的处理流程主要通过以下三个阶段的编译管线与运行时协同完成：

#### 1.被调函数的符号解离与克隆（GCC Pass 改造）

首先，编译器必须将父类中属于 GPU 世界的 `leaky` 函数从主机的符号体系中剥离。

- **AST 阶段感知**：AET 前端在进行词法与语法解析时，通过 `MtcsParser` 识别出 `leaky` 属于 mtcs（矩阵芯片系统）的专属异构函数。此时，编译器会为其创建 `ClassFunc` 对象，并将类型硬性标记为 `mtcs` 函数。
- **GIMPLE 拦截**：随后，`leaky` 函数的定义继续走 GCC 固有的 GIMPLE 中间表示生成流水线。AET 在 GCC 原生的 `*warn_function_noreturn` 这一 Pass 之前，强行插入了一个全新的自定义 GIMPLE Pass——`mtcs_collect_funcs`。
- **符号脱离**：在这个自定义 Pass 中，AET 会将 `leaky` 函数克隆作为一个独立的符号节点（`cgraph_node`）保存到新建 of 异构符号表 `symbol` 中。到这一步，设备端的 `leaky` 函数在编译器内部正式与原主机（Host）侧的普通函数脱离了关系，独立进入了异构后端的代码生成链。

#### 2.设备函数地址映射

代码解离了，但子类对象实例是在 CPU 端（Host）通过 `new$` 动态构建出来的，它在运行时启动 GPU 内核的瞬间，怎么在宿主端（Host）感知这个位于显存深处的父类函数地址？

这就是 AET 编译器最精妙的纽带设计——利用生成的变量作为媒介，实现运行时地址反向回填。如果我们去观测 AET 编译器最终吐出的真实 PTX 汇编代码，会看到这样一段关键定义：

```
.global .align 8 .u64 _Activation_deviceFuncPointers = { _Z10Activation5leakyEPN10ActivationEf };
```

编译器在生成的 PTX 文件中，专门定义了一个静态的**“设备方法映射表”**（即全局设备变量 `_Activation_deviceFuncPointers`），里面存放的就是经过名字修饰（Mangle）后的父类设备函数 `_Z10Activation5leakyEPN10ActivationEf` 的真实物理入口地址。

有了这个硬件层面的变量，主机的 CPU 又是怎么和它产生联系的呢？答案就在类对象的初始化方法里。当我们在 `main` 函数里实例化对象时，底层会触发由编译器自动生成的初始化函数 `Activation_init_object_1963020916`，其核心行为如下：

```
void * Activation_init_object_1963020916(Activation *self)
{
    if(self == NULL){
        return (void *)_createAClass_Activation_123((AObject *)self);
    }
    // ... 省略常规初始化 ...
    
    // 核心接线点：将“设备方法映射表”中的物理物理地址复制到主机端对象的成员变量中！
    mtcs_copy_device_func_address(
        (void*)&self->_Z10Activation5leakyEPN10ActivationEf, 
        "_Activation_deviceFuncPointers", 
        0, 
        self->mtcsPlatformType
    );

    // ...
    Activation_init_inner_super_data_2_Activation();
    return (void*)self;
}
```

这里的 `mtcs_copy_device_func_address` 是 AET 运行时（Runtime）提供给编译代码调用的底层驱动接口：它通过驱动层接口，把显存里**“设备方法映射表”**的 0 号元素（即父类 `leaky` 的物理执行地址）反向拉取出来，精准赋值给了主机端普通对象的内部成员变量 `self->_Z10Activation5leakyEPN10ActivationEf`。

紧接着，初始化方法最后会触发 `Activation_init_inner_super_data_2_Activation()`，通过一个原子锁保护，将代表该显存位置的字符串隐式挂载到了本地静态数组中：

```
_Activation__superDeviceAddressArray = (unsigned long)"0__Activation_deviceFuncPointers";
```

#### 3.解析并替换，super$ 调用转换

万事俱备，只欠最后一击。当子类 `MtcsActivation` 的核函数内部真正执行到 `super$->leaky(x)` 时，编译器中端如何把这一高级面向对象行为，翻译成合法的 PTX 跳转？

- **特征打标**：当编译器前端生成 AST 树时，发现这是一个 `super$` 跨执行空间调用，就会在树节点上加入一个特殊的标志：`AET_LANG_FLAG_5(func) == 1`。
- **符号强行重定向**：中端优化器在下发代码前，会扫描并进入专门处理该标志的函数 `super_call_replace_super_call`。在这里，原本昂贵、复杂的面向对象 `super$` 方法调用，在 AST 阶段，AET 将高级对象调用重新表达为设备函数地址调用，直接被替换为了对子类中收纳父类设备地址的数组变量 `_MtcsActivation_parent__superDeviceAddressArray` 的引用。

原本风马牛不相及的两个空间，在此处完成了闭环。

------

#### 总结：从对象模型到设备模型的跨越

回顾整个 AET 对 `super$` 的处理流程，我们可以清晰地看到，原本宿主 CPU 与异构 GPU 割裂的代码世界，是如何在程序员面前变得高度统一和互通的：

- **在高级语言层面**：程序员写下的是最符合软件工程直觉、极其自然的 `super$->leaky(x)`，享受到了面向对象带来的模块化封装和代码复用意义。
- **在编译器中端**：AET 通过自定义 GIMPLE Pass 强行将父类异构符号克隆并独立解离，并在最终生成树时对 `super$` 进行打标和降维替换。
- **在生成汇编层面**：后端（NVPTX）吐出了干净利落的 `_Activation_deviceFuncPointers` 硬件变量，不附带任何传统 CPU 面向对象运行时笨重的虚表（vtable）开销。
- **在运行时层面**：利用驱动地址映射动态把设备端函数的物理显存地址回填映射给主机侧的对象结构体成员，完成两个体系的无缝接线。

这就是编译器的魅力：将复杂的空间跨越和地址编织隐藏在冰山之下，把清爽、统一、好用的面向对象世界留给开发者。

------

### 四、 下一步面临的挑战

虽然 AET 编译器成功实现了统一抽象，但在提供这种高层级封装的同时，也让编译器承担了更多传统运行时系统的职责（如理解类继承关系、设备函数生命周期、平台映射以及运行时地址绑定），这导致其在未来面临以下四个维度的技术挑战：

1. **统一抽象可能隐藏硬件差异**：高层 OOP 抽象可能会让开发者忽视 GPU 线程束（Warp）对齐、分支分化（Branch Divergence）等底层物理特性，从而编写出不符合硬件特性的非合并内存访问代码。
2. **某些 GPU 高性能优化可能被抽象限制**：为了在编译期维持高层对象模型的完整性与拓扑结构，在某些极端场景下可能会限制后端的激进优化（如寄存器分配优化、循环展开与跨函数的指令级并行调度）。
3. **动态性和优化之间存在矛盾**：由于设备函数地址是在 Host 侧动态回填绑定的，如何在保障这种动态灵活性的同时，进一步让 GPU 后端编译器（如 NVVM/PTX）在编译期能够进行更深度的死代码消除（DCE）与静态内联（Inline）优化，是下一步演进的核心方向。
4. **编译器中间件的复杂度增加**：为了提供这种透明的统一抽象，编译器必须全权承担起传统的类继承拓扑分析、设备函数生命周期管理、多平台 ABI 映射以及运行时地址绑定的职责，增大了编译器中后端代码维护的工程复杂度。

