# AET Language

AET（Active Expandable Translator）是一门面向异构计算时代的系统级编程语言。

AET 不是对 C 语言增加几个面向对象关键字，也不是对 C++ 的重新实现。

AET 的目标是：

> **一次编写，多芯运行。**

在保持 C 语言高性能和 GCC 优化能力的基础上，AET 将面向对象、泛型编程和异构计算统一到同一个语言体系之中。

------

# 为什么需要 AET

过去几十年：

- C 解决了性能问题；
- C++ 解决了对象问题；
- Java 解决了大型软件工程问题；
- CUDA、OpenCL、HIP 等解决了部分异构计算问题。

但开发者仍然需要面对：

- CPU 一套编程方式；
- GPU 一套编程方式；
- AI 芯片又是一套编程方式；
- 不同平台之间代码难以复用；
- 编译器难以进行跨平台全局优化。

AET 希望改变这种局面。

AET 不把异构计算当作库。

而是把异构计算作为语言和编译器的一部分。

------

# AI 时代为什么还需要 AET

近年来，大语言模型已经能够帮助开发者：

- 编写代码
- 补全代码
- 修复 Bug
- 生成测试
- 重构程序

软件开发正在进入 AI 辅助时代。

但无论代码由人编写还是由 AI 生成，

最终都需要编译器完成：

- 语义分析
- 程序优化
- 平台适配
- 代码生成
- Runtime 组织
- 异构调度

特别是在异构计算时代，

编译器的重要性并没有降低。

反而变得更加重要。

------

## AI 负责生成代码

AI 更擅长回答：

> 应该做什么？

------

## AET 负责部署计算

AET 更关注：

> 在哪个芯片上运行？

> 如何运行得更快？

> 如何利用整个异构系统？

开发者或者 AI 只需要描述算法。

AET 负责将算法部署到：

- CPU
- GPU
- AI Accelerator
- 未来的新型计算设备

------

## 从源码编译到计算部署

传统编译器解决的是：

```
Source Code
    ↓
Machine Code
```

AET 希望进一步解决：

```
Algorithm
    ↓
Heterogeneous Computing System
```

未来的软件开发模式可能演变为：

```
Developer / AI
       ↓
      AET
       ↓
CPU + GPU + AI Chip
```

AET 相信：

> AI 负责生成代码，AET 负责把代码变成高效的计算。

------

# AET 的核心能力

## 面向对象

AET 支持：

- class$
- interface$
- abstract$
- extends$
- implements$
- genericblock$

帮助开发者构建大型软件系统。

------

## 保持 C 的效率

AET 基于 GCC 开发。

继承了 GCC 成熟的：

- 优化能力
- 代码生成能力
- 多平台支持能力

让开发者继续享受 C 语言的性能优势。

------

## 天生支持异构计算

AET 的设计目标不是只支持 CPU。

而是支持：

- CPU
- GPU
- DSP
- AI Accelerator
- 未来的新型计算芯片

开发者维护一份源码。

编译器负责生成对应平台代码。

------

## 与主流方案的对比

| 语言/框架         | C 兼容性 | 面向对象 | 异构计算支持      | 语法复杂度 | 编译器优化 | AET 的优势              |
| ----------------- | -------- | -------- | ----------------- | ---------- | ---------- | ----------------------- |
| **C**             | 原生     | 弱       | 需手动（CUDA 等） | 低         | 极强       | 现代 OOP + 原生异构支持 |
| **C++**           | 优秀     | 强       | CUDA / SYCL       | 高         | 强         | 更简单、关键字清晰      |
| **Rust**          | 一般     | 中       | 较弱              | 中高       | 强         | 更好 OOP + 异构原生     |
| **CUDA / HIP**    | 好       | 弱       | 仅单一 GPU 平台   | 中         | 中         | 跨平台一次编写          |
| **SYCL / oneAPI** | 好       | 中       | 多平台（有限）    | 中         | 中         | 语言级统一而非库级      |
| **AET**           | 原生     | 强       | **编译器原生**    | **低**     | 极强       |                         |





# AET 的创新

AET 不仅是一门语言。

同时也是一个面向异构计算的平台编译器。

传统 GCC 的编译流程：

```
Source
  ↓
Frontend
  ↓
GIMPLE
  ↓
RTL
  ↓
Assembly
```

AET 在 GIMPLE 与目标代码生成之间引入了面向异构平台的扩展层。

通过：

- Clone
- Port
- Expand

等机制，

将同一份程序扩展到多个目标平台。

因此：

> GCC 的 RTL 主要服务于单个平台，

而 AET 的 RTL 框架服务于多个计算平台。

这也是 AET 实现“一次编写，多芯运行”的基础。

------

# 已实现能力

当前版本已经实现：

- GCC 15.2.0 前端扩展
- 对象系统
- 泛型系统
- Eclipse CDT 集成
- PTX 后端

目前已经能够生成：

- NVIDIA GPU PTX 代码

下一阶段计划支持：

- AMD GCN
- SPIR-V

进一步覆盖：

- NVIDIA GPU
- AMD GPU
- Vulkan
- OpenCL

等主流异构平台。

------

# 实际应用

AET 已用于深度学习训练框架开发。

当前已实现：

- 卷积层（Convolution）
- 最大池化层（MaxPool）
- 平均池化层（AvgPool）
- 激活层（Activation）
- GPU Kernel
- 异构任务调度

在 CIFAR 图像分类训练测试中，

基于 AET 实现的训练框架，

相对于 darknet-alex 基线实现取得约 30%-40% 的性能提升。

------

# 示例

```
#include <stdio.h>

class$ Test{

    void hello();
};

impl$ Test{

    void hello(){
        printf("hello world\n");
    }
};

int main(){

    Test *t = new$ Test();

    t->hello();

    return 0;
}
```

------

### 实际工程案例：最大池化层（MaxPool）

```
public$ class$ MtcsMaxPoolLayer extends$ MaxPoolLayer {
    private$ InputData *input_antialiasing;
    // ...
}

impl$ MtcsMaxPoolLayer {
    void forwardMtcsMaxPool(NetworkState state) {
        forward_maxpool_layer_kernel <<<MtcsTool.gridSize(totalSize), MTCS_BLOCK, 0, MtcsTool.getStream() >>>
            (totalSize, ...);
    }

    __global__ void forward_maxpool_layer_kernel(...) { ... }
};
```





# AET 与 C++

AET 不追求成为另一个 C++。

AET 更关注：

- 简单
- 可扩展
- 易优化
- 面向异构计算

AET 希望：

> 保持 C 的效率，

> 获得现代软件工程能力，

> 面向未来异构计算平台。

------

# 安装

请查看：

```
INSTALL.md
```

------

# 项目状态

当前重点开发：

- 泛型系统
- PTX 后端
- GCN 后端
- SPIR-V 后端
- 异构 Runtime
- Eclipse CDT 插件
- 编译优化

------

# 愿景

未来的软件开发不应该围绕某一种芯片展开。

未来的编程语言也不应该只属于 CPU。

AET 希望构建一种真正面向异构计算时代的系统级语言。

让开发者关注算法和业务逻辑，

而把跨芯片优化与代码生成交给编译器完成。

------

# License

GPL v3

（与 GCC 保持一致）
