# AET Compiler Architecture

## Rethinking Program Semantics for the Era of Heterogeneous Computing

### Abstract

Since the emergence of high-level programming languages, compiler development has followed a consistent objective: to continuously raise the level of abstraction in programming so that developers can focus on algorithms rather than hardware details.

From machine code to assembly language, and later to high-level languages such as C and C++, compilers gradually took over responsibilities such as register allocation, instruction selection, calling conventions, and processor-specific optimizations. As a result, programmers could describe computational logic in a unified way without directly dealing with the underlying hardware.

However, as GPUs, matrix computing units (MTCS), NPUs, and other heterogeneous accelerators have become mainstream computing platforms, programs are increasingly executed across multiple execution environments simultaneously. This shift has changed not only the structure of computing resources but also the information that programs need to express.

Traditional compilers continue to follow the philosophy of hiding hardware differences by relying on runtime middleware or multiple toolchains to support heterogeneous systems. While this approach provides good compatibility, practical experience shows an increasing tension between the expressiveness of programming languages and the engineering complexity required to manage heterogeneous execution.

Based on the design and implementation of the AET compiler, this article explores whether heterogeneous computing requires introducing a new semantic dimension into programming languages:

**Execution Domain**.

------

# 1. The Evolution of Compiler Abstraction

The history of compiler development is fundamentally the history of raising the level of abstraction.

In the era of machine language, programmers had to work directly with processor instructions, registers, and memory addresses. Every hardware platform required its own programming model.

The introduction of high-level programming languages fundamentally changed this situation.

Languages such as C freed programmers from hardware-specific details. Developers only needed to describe computational logic, while the compiler became responsible for register allocation, instruction selection, calling conventions, and target-specific code generation.

At this stage, the central question of programming became:

> **What to Compute**

instead of

> **How to Execute on a Specific Hardware Platform**

Compilers successfully separated program design from hardware implementation by hiding most hardware-specific complexity behind a unified programming model.

------

# 2. Heterogeneous Computing Changes This Assumption

This abstraction was built upon an implicit assumption:

A program ultimately executes on a relatively unified computing platform.

Throughout the traditional CPU era, this assumption largely held true.

In the age of heterogeneous computing, however, it no longer does.

Modern applications may execute simultaneously on multiple types of computing devices, including:

- CPU
- GPU
- Matrix Computing Units (MTCS)
- NPUs and other specialized accelerators

As a result, computation is no longer confined to a single execution environment. Instead, it is distributed across multiple execution domains.

Consequently, a new question emerges:

> **Where Should the Computation Execute?**

This question was largely irrelevant in traditional CPU programming, but it has become a fundamental concern in heterogeneous computing.

# 3. How Traditional Compilers Handle Heterogeneous Computing

Traditional compiler infrastructures, represented by GCC, are designed to generate target-specific code while maintaining a unified programming model across different hardware architectures.

To support heterogeneous devices such as GPUs, GCC extends its capabilities through technologies like OpenMP, target offloading, and runtime scheduling. From the programmer's perspective, the programming model remains largely unchanged.

From an architectural standpoint, this approach continues the classic compiler philosophy:

> **Preserve the existing programming abstraction as much as possible, while encapsulating hardware differences within the compiler and the runtime system.**

This strategy offers excellent compatibility and minimizes changes to existing programming models. However, it also implies an important characteristic:

> **The execution location is not part of the program's semantics.**

Under this model, *where* a computation executes is determined by the runtime system rather than being explicitly represented by the program itself.

------

# 4. The AET Perspective

The design of AET originated from a long-term observation made during the development of heterogeneous compilers.

In practical high-performance computing applications—such as deep learning training, image processing, and scientific computing—developers no longer completely ignore hardware. Instead, they actively decide how computations should be distributed across different computing devices.

For example:

- Some computations are better suited for CPUs.
- Others achieve significantly higher performance on GPUs or matrix accelerators.
- The organization of data flow often has a direct impact on overall system performance.

In other words, **the execution location has already become part of the software design process in practice.**

However, within traditional programming semantics, this information remains implicit.

AET revisits this assumption by asking a fundamental question:

> **If the execution location has already become part of software design, should it also become part of the program's semantics?**

------

# 5. Execution Domain: A New Semantic Dimension

Based on these observations, AET introduces the concept of **Execution Domain**.

An Execution Domain represents the execution environment to which a computational unit is ultimately bound—for example, a CPU, GPU, or MTCS.

It is important to emphasize that an Execution Domain is **not** a hardware abstraction. Instead, it is a **semantic abstraction** that describes a fundamental property of a computation:

> **The class of execution environment in which a computation is intended to execute.**

In AET, for example, an object's methods can be bound to a specific execution domain through execution-domain annotations such as `__global__`.

These annotations are no longer treated merely as code generation hints. Instead, they become part of the program's semantic structure.

As a result, an AET program can explicitly express two orthogonal aspects of computation:

- **What to Compute**
- **Where to Execute**

Execution Domain therefore becomes an integral part of the language semantics rather than an implementation detail hidden inside the compiler or runtime system.

------

## 5.1 A Shift in the Programming Model

Introducing Execution Domain changes how a program is conceptually organized.

Developers no longer describe only the computational logic; they also explicitly specify the execution ownership of that computation.

This semantic shift has two important consequences:

- Execution boundaries become explicit in the program.
- The relationship between data and computation across execution environments is explicitly represented.

In traditional heterogeneous programming models, these decisions are typically deferred to the runtime system. In AET, they become part of the program's semantics and are established during compilation.

This allows the compiler to reason about execution placement directly from the program structure, rather than reconstructing it later through runtime analysis.

# 6. AET Compiler Architecture: Bringing Program Semantics into the Compilation Pipeline

In AET, the overall compiler architecture is built around the concept of **Execution Domain**.

The compilation pipeline can be summarized as follows:

```
Source Code
      │
      ▼
Object / Method Semantic Analysis
      │
      ▼
Execution Binding
      │
      ▼
Target-Specific Lowering
      │
      ▼
Backend Code Generation
      │
      ▼
CPU / GPU / MTCS
```

Within this architecture, the compiler's primary responsibility is no longer simply selecting a target platform. Instead, its central task is:

> **To map the Execution Domain expressed in the program's semantics to the appropriate execution backend.**

In other words, backend selection is no longer an isolated implementation detail. It becomes the natural consequence of semantic analysis performed earlier in the compilation process.

------

## 6.1 Semantic Analysis

During the frontend stage, the compiler is responsible for constructing the semantic representation of the program.

Its primary tasks include:

- Object modeling
- Method semantic analysis
- Parsing Execution Domain annotations
- Constructing execution bindings

Rather than merely parsing syntax, the frontend establishes the semantic relationships that determine where each computational unit is intended to execute.

------

## 6.2 Execution Binding

Once semantic analysis is complete, the compiler performs **Execution Binding**.

At this stage, it:

- Determines the Execution Domain associated with each computational unit.
- Establishes invocation relationships across execution domains.
- Prepares the data-flow structure required for cross-domain execution.

Execution Binding serves as the bridge between language semantics and backend code generation, preserving execution intent throughout the remainder of the compilation pipeline.

------

## 6.3 Target-Specific Lowering

During the backend stage, the compiler lowers each Execution Domain into its corresponding target-specific implementation.

For example:

- CPU backend
- GPU backend
- MTCS backend

Each backend is responsible for generating executable code for its target architecture while preserving the execution semantics established during the previous stages.

> Although this process is implemented collaboratively by multiple AET compiler modules, its underlying purpose remains the same: to translate the program's Execution Domain into concrete executable code for the corresponding execution environment.

# 7. Rethinking Compiler Abstraction

AET does not attempt to expose low-level hardware details, nor does it require programmers to understand the implementation specifics of heterogeneous execution.

Its objective is fundamentally different.

Rather than making hardware more visible, AET seeks to make **execution intent** explicit.

More specifically, AET introduces **execution location** as a first-class semantic concept without exposing the complexity of the underlying hardware.

Its central idea can be summarized as follows:

> **Allow programs to express where computation is intended to execute without requiring programmers to manage how it is executed on a particular hardware platform.**

From this perspective, AET does not reduce the level of abstraction established by traditional programming languages.

Instead, it extends that abstraction.

Traditional programming languages enable developers to describe **what** a program computes while leaving **how** the computation is carried out to the compiler.

AET preserves this principle while introducing an additional semantic dimension:

> **Execution Abstraction**

Execution Abstraction allows a program to express **where** computation belongs as part of its semantics, while the compiler remains responsible for translating that semantic intent into the implementation details of each target platform.

Consequently, the abstraction hierarchy evolves from a single-dimensional computational model into a richer semantic model:

- **What to Compute**
- **Where to Execute**
- **How to Execute** *(determined by the compiler for each execution backend)*

In this model, heterogeneous execution is no longer represented as a collection of backend-specific implementation techniques. Instead, it becomes a natural consequence of the program's semantics.

Execution Domain therefore is not merely a mechanism for backend selection. It provides a semantic foundation upon which heterogeneous compilation can be organized in a consistent and extensible way.

# 8. Conclusion

AET was not designed by starting from an abstract theory of programming languages. Instead, it evolved from practical experience in developing heterogeneous compilation systems.

Throughout this work, one recurring observation became increasingly evident:

> **In heterogeneous computing, the execution location of a computation is an essential part of program design, yet it remains largely absent from traditional program semantics.**

Conventional compiler frameworks address this problem through runtime scheduling, middleware, or offloading mechanisms. These approaches preserve a unified programming model by treating execution placement as an implementation concern rather than a semantic property of the program.

AET explores a different direction.

Instead of leaving execution placement entirely to the runtime system, AET introduces **Execution Domain** as an explicit component of program semantics. In doing so, execution intent becomes visible during compilation and can participate naturally in semantic analysis, execution binding, and backend code generation.

From this perspective, Execution Domain is not intended to be another hardware abstraction. Rather, it represents an extension of program semantics for the era of heterogeneous computing.

The evolution of programming languages has always been driven by the need to express computation at a higher level of abstraction. As heterogeneous architectures become increasingly prevalent, describing **where** computation belongs may become just as fundamental as describing **what** computation performs.

AET is an exploration of this possibility.

Rather than viewing heterogeneous compilation solely as a backend implementation problem, AET proposes that execution placement should be treated as a semantic property of the program itself.

Whether this semantic model proves valuable will ultimately depend on broader experimentation and practical applications. Nevertheless, AET demonstrates that heterogeneous compilation can be organized around program semantics, opening a different perspective on compiler architecture for heterogeneous computing.

## References

- **AET Compiler Project:** https://github.com/onlineaet/aet
- **AET-CNN Validation Framework:** https://github.com/onlineaet/aet-cnn