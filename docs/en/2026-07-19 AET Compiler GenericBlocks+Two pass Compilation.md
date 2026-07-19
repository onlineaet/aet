# AET Generics: Generic Blocks + Two-pass Compilation

## Introduction

Generics have become a fundamental feature of modern programming languages.

Today, most languages adopt one of two mainstream implementation strategies:

- **C++ Templates** (typically based on template instantiation / monomorphization)
- **Java Generics** (based on type erasure)

AET takes a different approach.

While developing the compiler, I arrived at a new implementation strategy:

> **Generic Blocks + Two-pass Compilation (Delayed Specialization)**

The goal is to preserve compile-time type capabilities without generating large amounts of duplicated code.

---

# Why Doesn't AET Use C++ Templates?

Most C++ compilers implement templates through **monomorphization**, generating a separate copy of the code for every concrete type.

### Advantages

- Full type information available at compile time
- Excellent optimization opportunities
- No runtime overhead

### Disadvantages

- Longer compilation times
- Larger binaries
- Code explosion when many template instantiations exist

AET aims to avoid this **template explosion**.

---

# Why Doesn't AET Use Java-style Type Erasure?

Java implements generics through **type erasure**.

### Advantages

- Excellent backward compatibility
- Small code size
- No duplicated generic code

### Disadvantages

- Concrete type information is discarded
- Operations depending on the actual type become limited

As a systems programming language, AET wants to retain both compile-time type safety and access to concrete type information, so type erasure is not suitable.

---

# The AET Approach: Delayed Specialization

AET tries to achieve the following:

> **Avoid both template explosion and type erasure.**

It does this by combining:

> **Generic Blocks + Two-pass Compilation (Delayed Specialization)**

### First compilation

- Generic types are temporarily represented as `void *`
- Generic metadata is preserved
- The source code of each Generic Block is stored

### Second compilation

When a concrete instantiation appears, for example:

```cpp
Abc<int>
```

the compiler

- reparses the corresponding Generic Block,
- replaces every occurrence of `E` with `int`,
- and generates specialized code only for that block.

---

# Comparison of Three Generic Implementations

| Feature | C++ Templates | Java Generics | AET |
|----------|---------------|---------------|-----|
| Strategy | Monomorphization | Type Erasure | Delayed Specialization |
| Code Generation | Separate copy per type | One shared implementation | Shared ordinary code, Generic Blocks specialized on demand |
| Type Information | Preserved at compile time | Erased after compilation | Preserved until the second compilation |
| Type-dependent Operations | Fully supported | Limited | Fully supported inside Generic Blocks |
| Compilation | One pass | One pass | Two passes |
| Binary Size | Large | Small | Small |
| Compilation Time | Longer | Shorter | Moderate |

---

# Generic Blocks: The Core of AET Generics

During the first compilation, ordinary generic code treats `E` as `void *`.

Only code inside a `genericblock$` undergoes specialization.

```cpp
genericblock$(a){
    E x = a;
    E y = 5;
    x += y;
}
```

```cpp
genericblock$(obj){
    E value = obj;
    value->walk();
}
```

Inside these blocks, the compiler must know the concrete type of `E`; otherwise it cannot generate correct code.

---

# Why Is a Second Compilation Necessary?

```cpp
void setData(E value)
```

Initially, the compiler only knows that `E` is a generic type, so it is compiled as `void *`.

When it later encounters:

```cpp
Abc<int>* a = new$ Abc();
```

it knows that `E = int`, reparses the stored Generic Block, and replaces every occurrence of `E` with `int`.

This is the essence of **Two-pass Compilation**.

---

# Compiler Implementation

1. Store generic metadata in the AST.
2. Preserve the original source code of each Generic Block.
3. Generate placeholder function calls during the first compilation.
4. Reparse the Generic Blocks during the second compilation.
5. Perform delayed specialization using the discovered concrete types.

---

# Summary

AET generics are neither C++ templates nor Java-style type erasure.

Instead, they are based on:

> **Generic Blocks + Two-pass Compilation (Delayed Specialization)**

Rather than specializing the entire generic function, AET specializes only the Generic Blocks that actually require concrete type information.

This approach attempts to balance several goals:

- Avoid template explosion
- Preserve full type capabilities
- Restrict specialization to the smallest possible scope

---

# Next Article

Once the compiler determines that `E = int`, how is this type information propagated through object references?

The next article introduces AET's **Object Reachability** algorithm for generic type propagation during the second compilation.
