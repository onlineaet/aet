# AET Compiler: How `super$` Works Across CPU/GPU Address Spaces

## 1. Introduction

Throughout the evolution of software engineering, object-oriented programming (OOP) has provided a programming model that makes complex systems easier to understand, extend, and maintain. One of its most important mechanisms is `super`, which allows derived classes to reuse and extend the behavior of their parent classes while keeping inheritance relationships explicit and well organized.

Traditional object-oriented systems, however, rely on an implicit assumption:

> **All objects and methods execute within a single, unified execution space (the host CPU).**

This assumption no longer holds in heterogeneous computing. A seemingly simple statement such as:

```cpp
super->method();
```

is no longer just a normal function call. It becomes a problem of **binding and locating methods across different address spaces and hardware execution environments.**

To address this challenge, the AET language introduces the `super$` keyword, supporting the following scenarios:

1. Calling a parent-class method that has the same name and parameters.
2. Invoking a parent-class constructor from a derived constructor.
3. Explicitly accessing parent-class member variables when names are shadowed.
4. Calling a parent device function (`__device__`) from a GPU kernel (`__global__`).
5. Calling a parent device function (`__device__`) from another device function (`__device__`).

The following program demonstrates this feature in the AET compiler.

```cpp
// Parent class: declares and implements a GPU device function
class$ Activation {
    __device__ float leaky(float x);
};

impl$ Activation {
    __device__ float leaky(float x) {
        return (x > 0) ? x : 0.1f * x;
    }
};

// Derived class
class MtcsActivation extends Activation {
    __global__ void compute(float x);
};

impl$ MtcsActivation {
    __global__ void compute(float x) {
        // The key challenge:
        // calling a parent device method from inside a GPU kernel.
        float r = super$->leaky(x);
        printf("super leaky: %f\n", r);
    }
};

int main() {
    MtcsActivation *tcs = new$ MtcsActivation();
    tcs->compute(5.0);
    return 0;
}
```

On a conventional CPU, compilers translate inheritance into ordinary function calls, static dispatch, or virtual table (vtable) lookups using a unified memory space and well-defined object layouts.

In heterogeneous computing, however, allowing `super$` to invoke parent methods from inside GPU kernels or device functions fundamentally changes the problem. The compiler must bridge the differences between host and device memory spaces, instruction sets, executable code, and hardware architectures while preserving the familiar object-oriented programming model.

------

# 2. Benefits for Developers

The AET compiler introduces a new compiler architecture that makes this mechanism possible, providing several practical benefits for heterogeneous application development.

## 2.1 Higher Productivity Through Simpler Heterogeneous Programming

In traditional CUDA C++ development, allowing a derived GPU operator to reuse logic from its parent class usually requires developers to manually refactor the parent implementation into standalone C-style functions.

This often involves:

- manually passing object pointers,
- explicitly forwarding parameters,
- copying data with `cudaMemcpy`,
- and maintaining separate kernel launch code.

As projects grow, even a small change to a parent interface may require updating every derived kernel manually.

With AET, developers simply write the most natural object-oriented code:

```cpp
float r = super$->leaky(x);
```

Behind this single line, the compiler automatically performs:

- symbol cloning,
- device-function address mapping,
- intermediate representation (IR) lowering,
- and runtime address binding.

The complexity of heterogeneous execution is hidden behind a familiar programming abstraction. Relationships between device functions, call paths, and address bindings are generated automatically by the compiler, allowing developers to focus on application logic instead of low-level infrastructure.

------

## 2.2 Bringing True Modularity to Large AI and HPC Software

Because of hardware constraints, many GPU software projects and high-performance operator libraries eventually degrade into procedural designs consisting of large collections of standalone functions, for example:

```cpp
conv_forward_gpu();

relu_forward_gpu();

pool_forward_gpu();
```

Such flat function collections become increasingly difficult to maintain as systems evolve.

Real-world software, however, is naturally organized as object hierarchies:

```text
Layer
 ├── ConvLayer
 ├── ActivationLayer
 └── PoolLayer
```

Objects naturally support inheritance, composition, and polymorphism.

The core contribution of AET is establishing an object-behavior mapping model for heterogeneous computing, making architectures such as:

```text
Layer
    └── GPU method
```

possible.

In other words, the GPU is no longer treated as an isolated accelerator. Instead, it becomes a first-class participant in the application's object model.

------

## 2.3 Compiler-Driven Abstraction with Minimal Runtime Overhead

A common concern is whether object-oriented abstractions introduce performance penalties on GPUs.

The design goal of AET is **not** to build another heavyweight runtime system.

Instead, the compiler resolves object relationships and device-function mappings during compilation. The expression

```cpp
super$->leaky(x)
```

is ultimately lowered into a device-function address call.

As a result, the implementation avoids the heavy runtime management typically associated with traditional object systems while preserving the familiar programming experience of object-oriented development.



# 3. How `super$` Is Implemented: Compiler Pipeline and Runtime Cooperation

The implementation of `super$` in the AET compiler is achieved through close cooperation between the compiler pipeline and the runtime system. The entire process can be divided into three major stages.

------

## 3.1 Symbol Separation and Cloning of Device Functions

The first step is to separate GPU device functions from the host-side symbol system.

When the AET front-end parses the source code, `MtcsParser` recognizes that `leaky` is a heterogeneous function belonging to the MTCS (Matrix Chip System) execution domain. A `ClassFunc` object is created for the method, and its type is explicitly marked as an MTCS function.

The function is then translated into GCC's normal GIMPLE intermediate representation. Before GCC executes its standard `*warn_function_noreturn` pass, AET inserts a custom GIMPLE pass named `mtcs_collect_funcs`.

Inside this pass, the compiler clones the device function and stores it as an independent `cgraph_node` in AET's heterogeneous symbol table (`symbol`).

At this point, the device implementation of `leaky` is completely detached from the original host-side function and enters an independent backend code-generation pipeline.

This separation establishes an important property:

> Device functions are no longer treated as ordinary C++ member functions. Instead, they become independent heterogeneous compilation units that can be compiled for GPU architectures while preserving their relationship with the original object hierarchy.

------

## 3.2 Mapping Device Function Addresses

Separating the code is only the first step.

A more difficult question remains:

The derived object is still created on the host by `new$`. When a GPU kernel is launched, how can this host-side object know the physical address of its parent's device function residing in GPU memory?

AET solves this problem through a compiler-generated device function mapping table.

The generated PTX contains a global variable similar to:

```ptx
.global .align 8 .u64 _Activation_deviceFuncPointers =
{
    _Z10Activation5leakyEPN10ActivationEf
};
```

This variable serves as a **device method mapping table**, storing the actual GPU entry addresses of all parent device methods after name mangling.

However, simply generating this table is not enough. The host object must somehow obtain these addresses.

To achieve this, the compiler automatically generates an initialization routine for every class.

When the following statement executes:

```cpp
Activation *obj = new$ Activation();
```

the compiler-generated initialization function is invoked automatically.

Its most important operation is:

```cpp
mtcs_copy_device_func_address(
    (void*)&self->_Z10Activation5leakyEPN10ActivationEf,
    "_Activation_deviceFuncPointers",
    0,
    self->mtcsPlatformType
);
```

This runtime API copies the physical GPU function address from the device mapping table into the corresponding member inside the host object.

In other words, although the object itself resides in host memory, it now contains the information required to locate the corresponding device implementation during kernel execution.

Finally, another compiler-generated initialization function records the mapping table identifier into a local static array protected by an atomic lock:

```cpp
_Activation__superDeviceAddressArray =
    (unsigned long)"0__Activation_deviceFuncPointers";
```

Rather than storing the device address directly, this metadata preserves the relationship between the object model and the generated device mapping table, allowing later compilation stages to resolve `super$` correctly.

------

## 3.3 Lowering `super$` into Device Calls

After symbol separation and address mapping have been completed, the final step is translating

```cpp
super$->leaky(x)
```

into valid PTX code.

This transformation is performed entirely inside the compiler.

### Step 1: Marking the AST

During AST construction, the front-end detects that this is a cross-execution-space `super$` invocation.

Instead of treating it as an ordinary inheritance call, AET marks the corresponding AST node with a dedicated flag:

```cpp
AET_LANG_FLAG_5(func) == 1
```

This flag identifies the expression as a heterogeneous `super$` call that requires special handling during later compilation stages.

------

### Step 2: Redirecting the Symbol

During middle-end optimization, the compiler scans for this flag before code generation.

Whenever such a node is encountered, the dedicated transformation routine

```cpp
super_call_replace_super_call()
```

rewrites the high-level object-oriented expression.

Instead of generating a conventional parent-class method invocation, the compiler replaces the call with a reference to the array inside the derived object that stores the parent device-function addresses:

```cpp
_MtcsActivation_parent__superDeviceAddressArray
```

At this point, the original object-oriented semantics have already been lowered into explicit device address references.

The GPU backend therefore no longer needs to understand inheritance or the `super$` keyword.

It only needs to generate code that performs an indirect device-function call through the resolved address.

This lowering strategy keeps the backend simple while preserving a natural object-oriented programming model at the language level.

Most importantly, it bridges two execution worlds that were previously isolated.

To developers, `super$` still behaves exactly like ordinary inheritance.

To the compiler, however, it has already become a carefully orchestrated sequence of symbol cloning, address mapping, AST rewriting, and backend code generation.



# 4. Conclusion: Bridging the Object Model and the Device Model

Looking back at the entire implementation of `super$`, we can see how AET unifies what were once two completely separate execution worlds—the host CPU and the heterogeneous GPU—behind a single programming abstraction.

At the language level, developers simply write intuitive object-oriented code such as:

```cpp
super$->leaky(x);
```

without worrying about address spaces, device symbols, or hardware-specific details. They benefit from the modularity, code reuse, and maintainability that object-oriented programming has provided for decades.

Behind this seemingly ordinary statement, however, the compiler performs a series of sophisticated transformations.

During the middle-end compilation stage, AET clones heterogeneous symbols through custom GIMPLE passes, separates device functions from the host compilation pipeline, and marks `super$` invocations for later lowering.

The backend then generates a clean device-function mapping table such as `_Activation_deviceFuncPointers`, avoiding the heavyweight runtime structures typically associated with traditional object systems, such as virtual tables (vtables).

Finally, at runtime, the compiler-generated initialization code retrieves the physical GPU addresses of device functions through the runtime driver and binds them back to the corresponding members of host-side objects. This completes the connection between the object model and the device execution model.

In other words, the entire mechanism can be summarized as follows:

- **Language level:** Developers write ordinary object-oriented code.
- **Compiler level:** AET performs symbol cloning, IR transformation, and call lowering.
- **Backend level:** Device function mapping tables are generated automatically.
- **Runtime level:** Device function addresses are dynamically resolved and bound to host objects.

The complexity of heterogeneous execution is hidden beneath these compilation stages, allowing developers to work with a unified programming model while the compiler transparently bridges different execution domains.

Ultimately, this is the value of a compiler.

Instead of exposing the complexity of heterogeneous hardware to programmers, the compiler absorbs that complexity internally and presents developers with a clean, consistent, and intuitive programming experience.

------

# 5. Future Challenges

Although AET successfully extends object-oriented abstractions across heterogeneous execution spaces, providing such a unified programming model also requires the compiler to assume responsibilities that traditionally belonged to the runtime system. These include understanding inheritance relationships, managing device-function lifecycles, maintaining platform mappings, and performing runtime address binding.

As the project evolves, several technical challenges remain.

### 1. Unified Abstractions May Hide Hardware Characteristics

High-level object-oriented abstractions make heterogeneous programming significantly easier, but they can also obscure hardware-specific behaviors such as warp execution, branch divergence, and memory coalescing.

Developers may unintentionally write code that appears elegant at the language level while performing poorly on GPU hardware.

Balancing abstraction and hardware awareness will remain an important challenge.

------

### 2. High-Level Abstractions May Restrict Certain Optimizations

To preserve object relationships and inheritance semantics during compilation, the compiler must maintain additional structural information throughout the compilation pipeline.

In some extreme cases, this may limit aggressive backend optimizations such as register allocation, loop unrolling, cross-function instruction scheduling, or other architecture-specific transformations.

Finding better ways to preserve abstraction without sacrificing optimization opportunities will be an important direction for future work.

------

### 3. Dynamic Binding Versus Static Optimization

In AET, device-function addresses are dynamically bound on the host before kernel execution.

While this provides flexibility, it also introduces tension between dynamic behavior and compile-time optimization.

An important future goal is enabling GPU backends such as NVVM/PTX to perform deeper static optimizations—including dead code elimination (DCE), function inlining, and interprocedural optimization—even when device addresses are resolved dynamically.

------

### 4. Increasing Compiler Complexity

Providing a transparent heterogeneous programming model inevitably increases the complexity of the compiler itself.

Instead of relying on a traditional runtime system, the compiler must now understand inheritance hierarchies, manage heterogeneous symbols, coordinate multiple execution platforms, generate platform-specific ABIs, and maintain runtime address mappings.

This significantly increases the engineering complexity of the compiler's middle-end and backend, making maintainability and extensibility increasingly important as the project grows.

------

## Final Thoughts

The goal of AET is not simply to make object-oriented programming available on GPUs.

Its broader objective is to explore whether a compiler can preserve familiar software engineering abstractions while automatically bridging fundamentally different execution domains.

The implementation of `super$` is one concrete step toward that vision.

Rather than forcing developers to adapt their software architecture to heterogeneous hardware, AET explores the opposite direction: adapting the compiler so that heterogeneous hardware can fit naturally into modern software architecture.

Whether this approach can evolve into a more general compiler model for heterogeneous computing remains an open question—but it is precisely this question that makes the exploration worthwhile.