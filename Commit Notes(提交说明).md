2026-11-05

##### 本次更新主要完善了 AET 的类对象生命周期管理和所有权规则，优化接口内部结构及 `ref/unref` 调用机制，并调整 `varof$` 的编译期实现方式。同时，增强了 MTCS 设备端支持，实现构造函数、析构函数及聚合类型返回值的设备代码生成，并修复了 `__global__` 与 CUDA 头文件的宏定义冲突。

1. **调整接口对象的内部数据布局**

   原来接口的第一个元素是一个公共数据结构：

   ```
   struct _IfaceCommonData123 {
       int   _aet_magic$_123;
       void *_atClass123;
   };
   ```

   现在不再嵌套该结构体，而是直接将两个成员放入接口结构中：

   ```
   int      _aet_magic$_123;
   AObject *_atClass123;
   ```

   同时明确 `_atClass123` 的类型为 `AObject *`。

2. **调整接口的 ref/unref 实现方式**

   删除接口原来定义的两个函数域 `ref`、`unref`。

   - 接口调用 `unref` 时，例如 `ifac->unref()`，编译器转换为 `ifac->_atClass123->unref()`。
   - 接口调用 `ref` 时，由编译器动态生成对应的函数定义，内部调用 `ifac->_atClass123->ref()`，并将结果转换为接口类型返回。

   这样可以减少接口自身的函数域，统一由底层 `AObject` 实现引用计数管理。

3. **调整 varof$ 的实现方式**

   删除 `AObject.h` 中原来为 `varof$` 关键字提供的实现函数，改为在编译过程中动态创建所需实现。

   由于 `varof$` 使用频率较低，这种方式可以减少不必要的函数定义，节省编译时间。

4. **支持设备端构造函数和析构函数**

   构造函数和析构函数现在支持使用 `__device__` 修饰，可以生成对应的设备端代码。

   进一步完善 AET 类对象在 CUDA 设备端的构造和析构支持。

5. **重新设计类变量的释放规则**

   重新梳理类变量的引用计数与释放机制，明确不同场景下的所有权关系。

   主要包括：

   - 明确 `new$` 创建的栈上对象的自动 cleanup 规则。
   - 普通赋值保持浅复制语义，不隐式改变引用计数。
   - 带有 cleanup 的局部对象作为函数返回值时，增加引用计数，保证局部对象释放后返回对象仍然有效。
   - 对函数调用返回的对象，在接收赋值时处理额外引用，避免引用计数失衡。
   - 区分所有权转移与普通借用，避免多个 cleanup 对同一引用重复释放。

   目前已经实现并测试普通赋值和声明初始化，例如：

   ```
   TFirst aytc;
   aytc = getdata();
   
   TFirst aytc = getdata();
   ```

   仍有一些复杂表达式场景尚未实现，例如：

   ```
   TFirst aytc = cond ? obj : getdata();
   
   getdata();
   
   getdata().xxxx;
   ```

6. **完善设备端聚合类型返回值支持**

   完善 GCC 对设备端聚合类型返回值的处理，使 AET 中的结构体等聚合类型能够正确生成设备端 PTX 代码。

   现在设备端函数不仅支持普通标量返回值，也能够正确处理 `int4` 等聚合类型的返回值。

7. **修复** `**__global__**` **与 CUDA 头文件的宏定义冲突**

   解决 CUDA 头文件中 `__global__` 宏定义对 AET 语法解析的影响，使 AET 能够正确处理相关声明，并保留自身的 `__global__` 语义。

2026-11-05

##### This update further improves AET's class object lifecycle management and ownership rules, optimizes the internal interface layout and `ref/unref` invocation mechanism, and changes how `varof$` is implemented at compile time. It also enhances MTCS device-side support by enabling device code generation for constructors, destructors, and aggregate return values, and fixes a macro definition conflict between `__global__` and CUDA headers.

1. **Adjust the internal data layout of interface objects**

   Previously, the first element of an interface was a common data structure:

   ```
   struct _IfaceCommonData123 {
       int   _aet_magic$_123;
       void *_atClass123;
   };
   ```

   The nested structure has now been removed. Its two members are placed directly in the interface structure:

   ```
   int      _aet_magic$_123;
   AObject *_atClass123;
   ```

   The type of `_atClass123` is also explicitly defined as `AObject *`.

2. **Change how** `**ref/unref**` **are implemented for interfaces**

   The `ref` and `unref` function members previously defined in interfaces have been removed.

   - When an interface calls `unref`, for example, `ifac->unref()`, the compiler translates it into `ifac->_atClass123->unref()`.
   - When an interface calls `ref`, the compiler dynamically generates the corresponding function definition. The generated function calls `ifac->_atClass123->ref()` internally and returns the result converted to the interface type.

   This reduces the number of function members in interfaces and centralizes reference counting in the underlying `AObject` implementation.

3. **Change the implementation of**  varof$

   The implementation function previously provided for the `varof$` keyword in `AObject.h` has been removed. The required implementation is now generated dynamically during compilation.

   Since `varof$` is used relatively infrequently, this approach avoids unnecessary function definitions and reduces compilation time.

4. **Support device-side constructors and destructors**

   Constructors and destructors can now be qualified with `__device__`, allowing the compiler to generate the corresponding device code.

   This further improves support for constructing and destructing AET class objects on CUDA devices.

5. **Redesign class variable cleanup rules**

   The reference-counting and cleanup mechanisms for class variables have been redesigned to clarify ownership in different scenarios.

   The main changes include:

   - Define the automatic cleanup rules for stack variables created with `new$`.
   - Preserve shallow-copy semantics for ordinary assignments without implicitly changing reference counts.
   - Increment the reference count when a local object with cleanup is returned from a function, ensuring that the returned object remains valid after the local object is cleaned up.
   - Handle the additional reference when assigning an object returned by a function, preventing reference-count imbalances.
   - Distinguish ownership transfer from ordinary borrowing to prevent multiple cleanup operations from releasing the same reference more than once.

   Ordinary assignments and declaration initializations have been implemented and tested, for example:

   ```
   TFirst aytc;
   aytc = getdata();
   
   TFirst aytc = getdata();
   ```

   Some more complex expression cases are not yet implemented, including:

   ```
   TFirst aytc = cond ? obj : getdata();
   
   getdata();
   
   getdata().xxxx;
   ```

6. **Improve support for device-side aggregate return values**

   Improve GCC's handling of aggregate return values in device code generation, enabling structures and other aggregate types in AET to generate valid device-side PTX code.

   Device functions can now return not only scalar values but also aggregate types such as `int4`.

7. **Fix the macro definition conflict between** `**__global__**` **and CUDA headers**

   Resolve the issue where the `__global__` macro defined in CUDA headers interferes with AET syntax parsing, allowing AET to correctly process relevant declarations while preserving its own `__global__` semantics.



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

