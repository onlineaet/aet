# AET Intrinsic Mechanism Design

## 1. Background

During the porting of llm.c to AET, we needed to handle CUDA-specific memory access instructions.

For example, `layernorm_forward_kernel3` contains the following call:

```
__ldcs(x + c);
```

Here, `__ldcs` comes from CUDA's `sm_32_intrinsics.hpp`. Its implementation is similar to the following:

```
__SM_32_INTRINSICS_DECL__
long __ldcs(const long *ptr)
{
    unsigned long ret;
    asm volatile (
        "ld.global.cs.s64 %0, [%1];"
        : "=l"(ret)
        : __LDG_PTR (ptr)
    );
    return (long)ret;
}
```

This function uses inline assembly to generate the CUDA PTX instruction `ld.global.cs.s64`, which performs a global memory load with the `.cs` cache policy.

Calling `__ldcs` directly solves the problem on CUDA, but the user code must include the corresponding CUDA intrinsic header to obtain the function declaration and implementation. This introduces a dependency on a platform-specific interface and makes it harder to use the same code across multiple target platforms.

MTCS is designed to support multiple computing platforms, including CUDA and GCN. If platform-independent code directly references CUDA intrinsic interfaces, switching to another platform requires changing those references and potentially modifying the calling logic.

To address this, AET introduces a new Intrinsic function mechanism: **platform-independent interface declarations are separated from platform-specific implementations, and the compiler locates and associates the appropriate implementation according to the current target platform.**

Users only need to call a unified interface. They do not need to include platform-specific intrinsic headers directly or know the assembly details inside the implementation classes.

## 2. Intrinsic Mechanism Design

AET originally supported three categories of functions:

1. Ordinary functions.
2. Builtin functions.
3. Internal functions (internal-fn).

This update introduces a fourth category: Intrinsic.

The Intrinsic mechanism uses class attributes to distinguish platform-independent interface classes from platform-specific implementation classes. This allows the compiler to locate and associate the appropriate implementation when generating code for a specific target platform.

### 2.1 Platform-Specific Implementation: `mii`

`mii` stands for **MTCS Intrinsic Implement**. It identifies a class that provides a platform-specific Intrinsic implementation.

For example, a CUDA implementation can be defined as follows:

```
public$ final$
__attribute__((mii("cuda", "sm_35")))
class$ CudaMemInstrinsic
{
public$ static __device__
float ldcs(const float *ptr)
{
    float ret = 5;
    asm volatile (
        "ld.global.cs.f32 %0, [%1];"
        : "=r"(ret)
        : "R"(ptr)
    );
    return ret;
}
};
```

The attribute `mii("cuda", "sm_35")` identifies the target platform and architecture for this implementation. The implementation is encapsulated in a static member function and defined in a header file visible to the compiler.

Other target platforms can provide their own implementations. Platform-specific assembly and implementation details remain inside the corresponding implementation classes rather than being exposed to user code.

### 2.2 Platform-Independent Interface: `mic`

`mic` stands for **MTCS Intrinsic Class**. It identifies a platform-independent Intrinsic interface class.

For example:

```
public$ __attribute__((mic()))
class$ Target
{
#pragma mtcs intrinsic
    public$ static __device__
    float ldcs(const float *ptr);
};
```

Here, `Target` is simply the name used for this example interface class; it is not a required name in the mechanism.

The class declares the unified calling interface without providing a concrete implementation. `#pragma mtcs intrinsic` marks the corresponding Intrinsic declaration.

When calling this interface, user code does not need to reference `CudaMemInstrinsic` directly or know the name of the platform-specific implementation class.

### 2.3 How the Compiler Associates Interfaces with Implementations

The Intrinsic mechanism involves three components:

- **User code:** Calls the platform-independent Intrinsic interface.
- **Platform-independent interface class (**`**mic**`**):** Declares unified static methods without providing concrete implementations.
- **Platform-specific implementation class (**`**mii**`**):** Identifies the target platform and architecture through its attributes and provides the corresponding static method implementations.

The compiler uses the current compilation target to locate the platform implementation that matches the platform-independent interface and associates the call with that implementation.

```
User Code
    |
    v
Platform-Independent Interface Class (mic)
    |
    v
AET Compiler
    |
    +---- Find CUDA implementation (mii)
    |
    +---- Find GCN implementation (mii)
    |
    +---- Find implementations for other target platforms (mii)
    |
    v
Generate code for the current target platform
```

This diagram illustrates the logical relationships. It does not mean that the compiler must search every platform sequentially. The actual implementation is selected according to the current compilation target.

With this mechanism, platform-independent code no longer directly depends on a target platform's intrinsic headers. The compiler locates and associates the platform-specific implementation, while users only need to work with the unified calling interface.

## 3. Design Goals

The mechanism addresses the following requirements:

1. **Platform decoupling:** Avoid direct dependencies on platform-specific intrinsic interfaces and headers, such as those provided by CUDA and GCN.
2. **Unified calling interface:** Provide a consistent way to invoke functionality across different target platforms.
3. **Compiler-managed association:** Locate and associate the appropriate implementation according to the current compilation target, without requiring users to select implementation classes themselves.
4. **Transparent implementation details:** Encapsulate platform-specific assembly and implementation details inside implementation classes, keeping them invisible to user code that uses the unified interface.
5. **Extensibility:** Support additional target platforms by providing corresponding implementation classes, without modifying platform-independent calling code.

The mechanism does not eliminate differences between hardware platforms. Instead, it encapsulates those differences in the platform-specific implementation layer and lets the compiler connect the unified interface to the appropriate implementation.

This is an important part of AET's approach to multi-platform compilation: **Write Once, Run on Multiple Chips.**