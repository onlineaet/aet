# AET Compiler in Practice: How to Turn a Class Method into a GPU Kernel

In my previous article, **"Introducing the AET Compiler: A Next-Generation Systems Programming Language for Heterogeneous Computing and the AI Era,"** I introduced the underlying architecture of the AET compiler. As a systems programming language designed for heterogeneous computing, AET is built around a simple vision:

> **Write once, run across multiple computing architectures.**

After publishing that article, I received many questions from developers interested in compiler internals and low-level programming. The most common one was straightforward:

> **What does heterogeneous programming in AET actually look like? As a developer, how do I write a real program that runs on a GPU?**

In this article, we'll answer that question using the programming world's familiar **"Hello World"** example. We'll build a simple GPU kernel step by step and see how AET bridges modern object-oriented programming with GPU backend code generation.

------

## 1. The Fragmentation of Traditional Heterogeneous Programming

Developing software for modern AI infrastructure often feels fragmented.

Suppose you want to implement a high-performance deep learning operator, such as an Attention kernel for large language models or an activation layer in a convolutional neural network. In a conventional CUDA-based workflow, you typically need to:

1. Write the host-side logic and memory management in C++.
2. Implement GPU kernels using CUDA C and the `__global__` qualifier.
3. Manually manage device memory using APIs such as `cudaMalloc()` and `cudaMemcpy()`, and launch kernels with the familiar `<<<grid, block>>>` syntax.

Although this programming model has proven effective, it introduces an obvious separation between the host and the device.

This library-based composition often leads to bloated software architecture. For example, a traditional C++ object cannot naturally represent a complex object whose behavior executes directly in GPU memory. As applications grow larger, maintaining a consistent object model across CPU and GPU becomes increasingly difficult, while compiler-driven cross-platform optimization also becomes more challenging.

**AET takes a fundamentally different approach.**

Instead of treating heterogeneous computing as an external programming library, AET integrates it directly into the language and the compiler itself. Heterogeneous execution is no longer an add-on API layered on top of C++; it becomes a first-class language feature that the compiler understands throughout the entire compilation pipeline.

------

## 2. Writing a GPU Kernel in AET

To support modern software engineering practices, AET introduces language constructs such as `class$` and `impl$`, while providing native support for GPU qualifiers including `__global__` for kernel functions and `__device__` for device functions.

Let's examine a real program used to validate the compiler. In AET, a GPU kernel can be declared directly as a member function of a class:

```
#include <stdio.h>

// 1. Declare a class containing a GPU kernel.
class$ HelloGPU
{
    __global__ void hello();
};

// 2. Implement the class method.
impl$ HelloGPU
{
    __global__ void hello()
    {
        printf("hello world from GPU\n");

        int id = threadIdx.x;   // Built-in GPU thread variable

        printf("thread id = %d\n", id);
    }
};

int main()
{
    // 3. Instantiate the object and launch the kernel as a class method.
    HelloGPU *gpu = new$ HelloGPU();

    gpu->hello<<<1, 2>>>();

    // Synchronize with the hardware.
    MtcsSystem.synchronize();

    return 0;
}
```

After passing through the AET compilation pipeline, this program generates NVIDIA PTX code. A simplified fragment is shown below:

```
...
.visible .entry _Z8HelloGPU5helloEPN8HelloGPUE (.param.u64 %in_ar0)
{
    .reg.u64 %ar0;
    ld.param.u64 %ar0, [%in_ar0];
    .local .align 8
...
}
```

Running the program produces the following output:

```
hello world from GPU
hello world from GPU
thread id = 0
thread id = 1
```

Although this example is intentionally simple, it demonstrates one of AET's core ideas:

**A GPU kernel is no longer required to be a standalone global function. Instead, it can be expressed naturally as part of an object's behavior.**

The programmer continues to think in terms of classes, objects, and methods, while the compiler is responsible for transforming those abstractions into executable GPU code.

------

## 3. How Do Objects Cross CPU and GPU Execution Spaces?

The previous example demonstrated how a class method can be compiled into a GPU kernel. However, this is only the first step toward object-oriented heterogeneous programming.

For real-world applications—such as AI network layers, computation graphs, or high-performance operators—simply generating a standalone kernel is far from sufficient.

A more fundamental question arises:

> **How can an object-oriented instance naturally span two completely separate execution spaces: the CPU and the GPU?**

This is one of the core challenges that every heterogeneous programming language must address.

### 3.1 The Separation Between Objects and Data in Traditional Heterogeneous Programming

In conventional CUDA C++ programming, the **object itself** and the **data used by the GPU** are usually separated.

Consider a typical AI layer:

```
class Layer {
    float *weight;      // Stored in device memory
    int batch_size;     // Stored in host memory

    void forward();
};
```

The object lives in host memory, while its computationally intensive method (`forward()`) executes on the GPU.

Making this work requires developers to write a significant amount of synchronization code between the Host and the Device. Object state must be unpacked manually, copied into GPU memory, and reconstructed as raw pointers before the kernel can execute.

As a result, many of the advantages of object-oriented programming—including encapsulation and inheritance—become difficult to preserve across heterogeneous execution boundaries.

**AET fundamentally changes this programming model.**

Instead of forcing developers to adapt their code to hardware limitations, AET lets the compiler and runtime determine how objects should be represented across different execution environments.

### 3.2 Compiler Analysis and Runtime Allocation

Object creation in AET looks completely ordinary:

```
Object *obj = new$ Object();
```

Behind this simple statement, however, the compiler performs object and dependency analysis to determine whether the object participates in heterogeneous execution.

Based on that analysis, AET automatically selects one of two allocation strategies.

#### Objects Used Only by the CPU

If the class contains only ordinary host-side logic and does not participate in any `__global__` or `__device__` execution, the object is allocated using the standard CPU heap.

No additional runtime overhead is introduced.

#### Objects Used in Heterogeneous Execution

Once an object is detected as part of a heterogeneous execution context, the AET runtime takes over.

On the current NVIDIA CUDA backend, such objects are automatically allocated using **Unified Memory**, allowing both the CPU and GPU to access the same object state without requiring developers to explicitly manage memory transfers.

The selection of the allocation strategy is entirely transparent to the programmer.

### 3.3 Stack Objects and Automatic Deep Copy

Developers frequently create objects on the stack as well:

```
Object obj = new$ Object();
```

In traditional heterogeneous programming, GPU kernels cannot directly access data residing in a CPU thread's stack frame.

To preserve natural programming semantics, AET performs an automatic **deep copy** whenever a stack object enters GPU execution.

At kernel launch, the runtime creates a corresponding object in device memory and copies the complete member layout from the host stack to the device representation.

This process is performed automatically, allowing stack objects to participate in heterogeneous execution without requiring additional user code.

Because of this design, object-oriented syntax remains unchanged inside GPU kernels.

```
class$ HelloGPU
{
    int value;

    __global__ void hello();
};

impl$ HelloGPU
{
    __global__ void hello()
    {
        printf("Value from GPU context: %d\n", self->value);
    }
};
```

Notice the use of `self`.

Even though execution has moved from the CPU to the GPU, `self` continues to represent the current object instance.

Developers no longer need to unpack host objects, copy individual members with `cudaMemcpy()`, or pass long lists of primitive parameters into kernels.

From the programmer's perspective, object-oriented semantics remain consistent across execution spaces.

------

## 4. Current Boundaries and Future Directions

Although AET unifies much of the heterogeneous programming model, CPUs and GPUs remain fundamentally different execution environments.

Consequently, AET currently defines clear boundaries for object access inside GPU code.

Within `__global__` and `__device__` functions, developers may freely access:

- Object member variables
- Other GPU-callable methods (kernel or device functions)

However, ordinary CPU methods cannot be invoked directly.

For example:

```
class$ Test
{
    void cpuFunction();

    __device__ void gpuFunction()
    {
        cpuFunction();      // Compilation error
    }
};
```

This restriction is not an arbitrary language limitation.

Rather, it reflects the underlying hardware architecture.

A host function is compiled into CPU instructions (such as x86 or ARM), while GPU code is compiled into an entirely different instruction set. Since the corresponding CPU instructions simply do not exist within the GPU execution environment, the hardware cannot execute them.

In other words, this boundary is imposed by the execution model of heterogeneous hardware rather than by the language itself.

### Looking Forward

Although AET already enables object-oriented programming across heterogeneous systems, building a fully transparent object model remains an ongoing area of research.

Some of the challenges currently under investigation include:

- Lower-overhead object lifetime management across heterogeneous devices.
- Fully automatic synchronization of object state between the CPU and GPU.
- More natural support for complex object models inside GPU kernels.

These challenges are not unique to AET.

They represent fundamental problems that future heterogeneous programming languages and AI infrastructure must solve in order to provide developers with a truly unified programming model.

------

## References

**AET Compiler**

https://github.com/onlineaet/aet

**AET-CNN Validation Project**

https://github.com/onlineaet/aet-cnn