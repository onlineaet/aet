2026-10-14

本次提交总结了过去三个星期的主要开发成果，重点推进了 AET 编译器的 Intrinsic 函数机制，以及 MTCS 对内存操作函数的支持。

1. 新增 AET Intrinsic 函数机制，在普通函数、内建函数（builtin）和内部函数（internal-fn）之外，引入第四种函数类型：Intrinsic。
2. 实现基于平台名称和目标架构属性的 Intrinsic 实现机制，允许在类的静态成员函数中直接使用目标平台汇编，封装平台相关实现。
3. 实现平台无关的 Intrinsic 调用机制，将平台无关的接口声明与目标平台的具体实现分离，由编译器查找并关联相应的目标实现，减少用户代码对具体平台的依赖，推进“编写一次，多芯运行”。
4. MTCS 新增对 `memcpy` 和 `memset` 两个 C 函数的支持，目前仅实现 CUDA 平台。

###### 详见 `docs/zh/2026-10-14 提交说明.md`

---

2026-10-14

This commit summarizes the major development work over the past three weeks, focusing on the new AET Intrinsic mechanism and memory-operation support in MTCS.

1. Introduced the AET Intrinsic function mechanism, adding a fourth function category alongside ordinary functions, builtins, and internal functions (internal-fn).
2. Implemented a platform- and architecture-aware mechanism for Intrinsic implementations, allowing target-specific assembly to be encapsulated in static member functions.
3. Implemented a platform-independent Intrinsic calling mechanism that separates interface declarations from target-specific implementations. The compiler locates and associates the appropriate target implementation, reducing platform dependencies in user code and advancing AET's goal of **“Write Once, Run on Multiple Chips.”**
4. Added MTCS support for the C functions `memcpy` and `memset`. Currently, these implementations are available only for CUDA.

###### See `docs/en/2026-10-14 Commit Notes.md` for details.

---

2026-09-09

本次提交为过去三个星期的主要开发成果，主要完成了 AET 泛型系统、泛型容器以及编译器相关问题的进一步完善。

1. 实现泛型 APriorityQueue
2. 实现泛型三级函数内联
3. 完善 AArray 泛型容器
4. 修复泛型返回值问题
5. 修复泛型常量参数生命周期问题

###### 详见 `docs/zh/2026-09-09 提交说明.md`

---

2026-09-09

This commit summarizes the major development work over the past three weeks, primarily focusing on further improvements to the AET generic system, generic containers, and compiler-related issues.

1. Implemented the generic APriorityQueue
2. Implemented three-level function inlining for generics
3. Improved the AArray generic container
4. Fixed issues with generic return values
5. Fixed the lifetime issue of generic constant arguments

###### See `docs/en/2026-09-09 Commit Notes.md` for details.

2026-09-09

本次提交为过去三个星期的主要开发成果，主要完成了 AET 泛型系统、泛型容器以及编译器相关问题的进一步完善。

1. 实现泛型 APriorityQueue

2. 实现泛型三级函数内联

3. 完善 AArray 泛型容器

4. 修复泛型返回值问题

5. 修复泛型常量参数生命周期问题

###### 详见docs/zh/2026-09-09 提交说明.md



2026-09-09

This commit summarizes the major development work over the past three weeks, primarily focusing on further improvements to the AET generic system, generic containers, and compiler-related issues.

1. Implemented the generic APriorityQueue
2. Implemented three-level function inlining for generics
3. Improved the AArray generic container
4. Fixed issues with generic return values
5. Fixed the lifetime issue of generic constant arguments

###### See `docs/en/2026-09-09 Commit Notes.md` for details.

