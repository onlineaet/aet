# 2026-09-09

This commit summarizes the major development work completed over the past three weeks, focusing primarily on further improvements to the AET generic system, generic containers, and compiler-related issues.

### 1. Implemented Generic APriorityQueue

Implemented the generic `APriorityQueue` container with support for both `MinHeap` and `MaxHeap`, using a unified comparator mechanism to implement both heap types.

Under `-O3` optimization, a test with 1,000,000 elements showed that Push performance is broadly comparable to C++ `std::priority_queue`. After optimizing Pop, the performance gap was further reduced. In some test scenarios, the overall performance was approximately 3%–7% faster than C++ `std::priority_queue`.

Test programs:

```text
test/test-priorityqueue.c
test/test-priorityqueue1.cpp
```

### 2. Implemented Three-Level Function Inlining for Generics

Further improved the code generation and inlining mechanism of AET generic blocks, enabling the following three-level call chain:

```text
fwgb (function containing a generic block)
    ↓
genericblock$
    ↓
compareFunc (comparator function)
```

For example:

```c
impl$ APriorityQueue
{
   ...
   public$ void push(E x) {
      genericblock$(x){
         ...
         if(((ProrityCompare)compareFunc)(&x,&e))
            break;
         ...
      };
   }
   ...
};
```

Invocation:

```c
APriorityQueue<int,MinHeapFunc> *apq =
    new$ APriorityQueue(N, intComparator_cb);

for (int i = 0; i < N; ++i)
    apq->push(data[i]);
```

After the concrete types are determined, the compiler generates a specialized generic-block function, for example:

```c
static inline void
_int_0_MinHeapFunc_0_aet_util_APriorityQueue__gen_block_func_1(
    aet_util_APriorityQueue *self,
    aet_generic_E _aetGenNewParamPrefix_x)
```

This ultimately allows:

```text
apq->push(data[i])
    ↓
push
    ↓
genericblock
    ↓
compareFunc
```

to be inlined into the same optimization context, enabling multi-level inlining of generic code.

This represents a further practical application of AET's **Delayed Specialization** mechanism in generic containers.

### 3. Improved the AArray Generic Container

Further improved `AArray` by adopting a three-pointer storage model similar to C++ `std::vector`:

```text
start
finish
end_of_storage
```

Added and improved:

- `resize`
- `set`
- Pre-allocation
- Element count calculation
- Bounds checking

Extensive performance tests were also performed.

In a test involving 100 million element operations on `AArray<int>`, the optimized implementation achieved performance comparable to, and in some cases better than, the corresponding C++ `std::vector` tests.

### 4. Fixed Generic Return Value Handling

Fixed the following case:

```c
E getData(){
   return NULL;
}
```

When `E` is erased to `void *` in a non-generic-block context:

```c
return NULL;
```

is valid.

However, when `E` is instantiated as a value type during the second compilation stage, returning `NULL` results in incorrect semantics.

AET now transforms it into:

```c
generic_is_pointer(E)
    ? NULL
    : _aet_generic_zero_storage
```

After generic instantiation, `generic_is_pointer(E)` can determine whether `E` is a pointer type or a value type.

`_aet_generic_zero_storage` is a global zero-initialized storage area whose size is determined by the maximum size of all generic value types used throughout the project.

Additionally:

- `_aet_generic_zero_storage` in the `.so` uses a weak symbol.
- The final executable provides a strong symbol.

This allows the definition in the final executable to override the definition provided by the `.so`.

### 5. Fixed the Lifetime of Generic Constant Arguments

Fixed the following case:

```c
void setData(E value);

setData(5);
```

Because the generic parameter `E` is erased to `void *` during the first compilation stage, the constant `5` needs to be converted into the address of a temporary object:

```c
setdata(({ int a = 5; &a; }));
```

The original implementation could result in:

```text
warning: using a dangling pointer to ‘a’
```

The compiler now generates a temporary variable:

```text
_temp_generic_const_var_xxx
```

inside the body of the current function and inserts it before the first statement of the function body, ensuring that the temporary object has a sufficiently long lifetime.

The relevant implementation is located in:

```text
generic-typeck.c
modifyCst()
```

### Summary

The main focus of these three weeks of development was not simply adding several containers, but further improving AET's generic compilation model:

```text
Generic Type
     ↓
Type Erasure
     ↓
Delayed Specialization
     ↓
Generic Block Generation
     ↓
Multi-level Inlining
     ↓
Concrete Optimized Code
```

The work also further improved the runtime implementation of generic containers and addressed generic value types, generic return values, the lifetime of generic constant arguments, and related compiler issues.