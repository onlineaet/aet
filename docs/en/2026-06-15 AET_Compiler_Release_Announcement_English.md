# AET Compiler Official Release: A New Generation of System-Level Programming Language for Heterogeneous Computing and the AI Era

### ------ Write Once, Run on Multiple Chips; Let the Compiler Connect CPU, GPU and Future AI Accelerators

After long-term development and testing, today we officially release the
**AET (Active Expandable Translator) Compiler project**.

AET is a new compiler system built on GCC, and also a system-level
programming language designed for heterogeneous computing.

Its goal:

> Allow developers to build high-performance programs with a unified
> language model, and use the compiler to expand code execution to CPUs,
> GPUs, and future AI acceleration devices.

Project links:

AET Compiler: [GitHub - onlineaet/aet: AET (Active Expandable
Translator) is a system-level programming language for the era of
heterogeneous computing. AET is not just adding a few object-oriented
keywords to C, nor is it a reimplementation of C++. Its goal is: write
once, run on multiple chips. Based on C performance and GCC optimization
capabilities, AET unifies object-oriented programming, generic
programming, and heterogeneous computing into one language system. ·
GitHub](https://github.com/onlineaet/aet)

AET-CNN: [GitHub - onlineaet/aet-cnn: A convolutional neural network
developed with the AET language ·
GitHub](https://github.com/onlineaet/aet-cnn)

------------------------------------------------------------------------

### Why Do We Need a New Compiler?

Over the past decades, software development has gone through several
stages:

C solved system performance problems.

C++ provided large-scale software engineering capabilities.

CUDA enabled general-purpose GPU computing.

Python and AI frameworks lowered the barrier for artificial intelligence
development.

But today's computing environment is changing.

A modern AI program usually contains:

    Python algorithm code
           ↓
    C/C++ underlying framework
           ↓
    CUDA GPU Kernel
           ↓
    Hardware execution

Developers need to handle:

-   CPU programming models
-   GPU programming models
-   Different hardware APIs
-   Different optimization methods

As GPUs, NPUs, and AI chips continue to increase, the distance between
software and hardware is growing.

AET explores another direction:

> Make heterogeneous computing a compiler capability instead of
> requiring developers to manually combine different languages.

------------------------------------------------------------------------

### Core Idea of AET

Traditional development model:

    Application
     |
     +---- CPU code
     |
     +---- GPU code
     |
     +---- Accelerator code

Different hardware requires different languages and toolchains.

AET aims to achieve:

              AET Source
                 ↓
              AET Compiler
                 ↓
        -------------------
        ↓         ↓       ↓
       CPU      GPU    AI Chip

Developers focus on algorithms and program logic.

The compiler handles:

-   Code generation
-   Optimization
-   Hardware adaptation

------------------------------------------------------------------------

### AET: A New Compiler System Based on GCC

AET does not implement a simple interpreter. Instead, it deeply
integrates with the GCC compilation system.

The current version is based on:

-   GCC 15.2
-   GIMPLE intermediate representation
-   RTL backend
-   NVPTX GPU backend

Traditional GCC:

    Source
     ↓
    Frontend
     ↓
    GIMPLE
     ↓
    RTL
     ↓
    Machine Code

AET extension:

    AET Source
     ↓
    AET Frontend
     ↓
    GCC IR
     ↓
    Backend
     ↓
    CPU / GPU Code

This means:

AET programs are not interpreted at runtime.

They go through a complete compilation process and generate target code.

------------------------------------------------------------------------

### AET Language: Keeping C Performance, Adding Modern Language Features

AET keeps C language characteristics:

-   High performance
-   Close to hardware
-   Easy optimization

At the same time, it adds modern software engineering capabilities:

-   class
-   interface
-   inheritance
-   generic programming

Example:

``` cpp
class$ Network {
    void train();
};

impl$ Network {
    void train(){
        printf("training\n");
    }
};
```

Goal:

Make system-level development have both:

C performance and the organizational capabilities of modern languages.

------------------------------------------------------------------------

### Language Design for Heterogeneous Computing

Traditional CUDA development:

    CPU code
    +
    GPU Kernel

Developers need to manually manage:

-   Data transfer
-   Kernel design
-   Platform differences

AET attempts to bring GPU computing into the language system.

Compilation flow:

    AET program
         ↓
    AET Compiler
         ↓
    PTX
         ↓
    NVIDIA GPU

Currently implemented:

-   NVIDIA PTX generation
-   GPU Kernel support
-   CUDA ecosystem execution

Future plans:

-   AMD GPU
-   SPIR-V
-   More AI acceleration platforms

------------------------------------------------------------------------

### AET-CNN: Developing a Real AI Application with a New Language

To verify that AET is more than a language experiment, I developed:

##### AET-CNN Image Classification Training Framework

Includes:

-   Convolution layers
-   Pooling layers
-   Activation functions
-   GPU Kernel
-   Complete training pipeline

Architecture:

            AET-CNN
                ↓
            AET Language
                ↓
            AET Compiler
                ↓
            CPU/GPU

Test task:

CIFAR image classification

Test environment:

-   NVIDIA GTX 1650

Performance test:

In the author's test environment:

Compared with Darknet-Alex:

AET-CNN training speed improved by:

**30%～40%**

This result verifies:

Programs generated through a new language and compiler system can be
applied to real AI workloads.

------------------------------------------------------------------------

### From a Language Experiment to a Complete Compiler Ecosystem

AET development involves:

-   GCC frontend extension
-   Type system design
-   GIMPLE processing
-   Backend code generation
-   PTX analysis
-   GPU performance optimization

Behind a simple language feature, many aspects must be considered:

-   Semantics
-   Intermediate representation
-   Optimization
-   Target hardware

What the compiler truly connects is:

    Developer ideas
          ↓
    Programming language
          ↓
    Compiler
          ↓
    Hardware computing capability

------------------------------------------------------------------------

# Software Foundation for Future AI Computing

The AI era is changing software development.

In the future:

AI can help generate code.

But:

Where does the code run?

How should it be optimized?

How can hardware be fully utilized?

Ultimately, the compiler is still responsible for completing this.

AET's exploration direction:

> AI creates algorithms, and the compiler makes algorithms run
> efficiently.

Welcome to follow and participate in:

-   Compiler development
-   GPU computing
-   AI infrastructure
-   New programming languages

**Overall AET architecture diagram:**

![img](https://i-blog.csdnimg.cn/direct/95216dbf97d54a8bb28f4bdad2015230.png)
