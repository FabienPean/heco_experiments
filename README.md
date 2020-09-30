# HeCo - Heterogeneous Containers in C++

## TL;DR

This repository provides several experimental skeletons of heterogenous containers. Something similar in the spirit to a  `std::vector<std::any>` but with different properties and generally much faster on many (all?) aspects.

## Generic example

```cpp
using HeterogeneousContainer = /*...*/;
HeterogeneousContainer hc;
hc.insert(42);
hc.insert(std::vector<int>{});
hc.get<std::vector<int>>().push_back(404);
assert(hc.get<int>() == 42);
assert(hc.get<std::vector<int>>().at(0) == 42);
```

## Building blocks

- `data` the object
- `ptr` where the object is stored, a pointer or offset allowing to find the object in memory
- `tag` what is the type of that object, a unique way to identify types at runtime.
- `dtor` how to destroy the object, necessary to avoid memory leaks

A huge variety of containers can be built out of the organization of these in different data structure. Furthermore, simplification and specialization increase dramatically the number of variant possible. For example, the memory address of the destructor is itself characteristic of a type, and thus `tag` and `dtor` could be combined in some situations.

## Containers

### Nomenclature

`heco_1_map_array` means an heterogeneous container [heco] that can hold only one instance of each type [1] which relies on a *map* interface to retrieve the instance out [map], and which is stored in a contiguous array [array]

### [heco_1_map_array]

A container which stores all instances in a single buffer to ensure memory contiguity. 

```cpp
std::vector<std::byte> objects;
Map<tag_t, offset_t> offsets;
Map<tag_t, dtor> destructors;
```

### [heco_1_map_stable] 

A container where instances are stored within a type-erased unique pointer, providing de facto stable pointer to the object.

```cpp
using ptr_dtor = std::unique_ptr<void,void(*)(void*)>;
Map<tag, ptr_dtor> data;
```
### [heco_1_sparseset_stable]

A container relying on a sparse set. Requires to have a `tag` generated sequentially

```cpp
using ptr_dtor = std::unique_ptr<void, void(*)(void*)>;
struct any { tag_t tag; ptr_dtor ptr; };
using index = std::uint8_t;
std::vector<index> sparse;
std::vector<any> data;
```
### [heco_n_map_stable]

A container generalizing `heco_1_map_stable` to any number of instances of each type. Any inserted type is stored in a `std::vector`

```cpp
    using ptr_dtor = std::unique_ptr<void, void(*)(void*)>;
    std::unordered_map<tag_t, ptr_dtor> data;
```
### [heco_n_map_vector]

A container specialized to store any number of instances of each type in dynamic arrays. Specific position of an instance is lost in the process. Using a handmade vector type (te_vector) allows to remove one indirection compared to `heco_n_map_stable` (`heco_n_map_stable`:tag&rarr;unique_ptr&rarr;vector&rarr;data vs `heco_n_map_vector`:tag&rarr;te_vector&rarr;data)

```cpp
std::unordered_map<tag_t, te_vector> data;
```

## Motivation

One sees regularly questions or post popping-up online about the existence or the desire of an heterogenous container in C++, where the user can store any type within.

1. [A true heterogeneous container in C++](https://gieseanw.wordpress.com/2017/05/03/a-true-heterogeneous-container-in-c/) 
2. [c++ heterogeneous container, get entry as type](https://stackoverflow.com/questions/47768354/c-heterogeneous-container-get-entry-as-type)
3. [Who else would like to see multi-type lists?](https://www.reddit.com/r/cpp/comments/cniy5y/who_else_would_like_to_see_multitype_lists/)
4. [Proof of concept runtime heterogeneous containers](https://www.reddit.com/r/cpp/comments/dnyneq/proof_of_concept_runtime_heterogeneous_containers/)
5. [Multi type vectors](https://www.reddit.com/r/cpp_questions/comments/fjcdvz/multi_type_vectors/)

Comments are often centered around the use  of `vector<any>`, `vector<variant>`. However:

* With `vector<any>`, *any* bundles together everything together to manipulate a type (tag, ptr, dtor, etc) and is relatively slow.
* With `vector<variant>`, *variant* forces the user to provide a list of types known at compile-time that the variant can hold. Moreover, memory is inherently wasted because the size of a variant is the size of the largest possible type it can hold plus some extra for the type identifier.

In this [post](https://gieseanw.wordpress.com/2017/05/03/a-true-heterogeneous-container-in-c/), an heterogeneous container is presented that relies mostly on `static` storage to retrieve the requested value using the address in memory of the container's instance as a key in a map, see below. All elements of the same type are stored contiguously, which favors fast per-type linear iteration.

```cpp
template<class T>
static std::unordered_map<const heterogeneous_container*, std::vector<T>> items;
```

Since I was kind of frustrated by these solutions, I decided to explore the design space of heterogeneous containers. I used this exercise as a good opportunity to experiment with modern C++ standard ($\geq$17) and low-level memory manipulation.

## Elsewhere

[DataFrame](https://github.com/hosseinmoein/DataFrame) is a C++ implementation of a [class](https://pandas.pydata.org/pandas-docs/stable/reference/api/pandas.DataFrame.html) of the same name in the [pandas](pandas.pydata.org) python library. The main data structure under the hood is a container inspired by the [post]() given above.

[EnTT](https://github.com/skypjack/entt) is an [Entity-Component-System](https://en.wikipedia.org/wiki/Entity%E2%80%93component%E2%80%93system) library (among other things) with top-notch performance. Its backbone is the registry class, which relies on type-erasure and a sparse set data structure as underlying container for components (objects). When one inserts a component of type *C* into the registry, these instances are packed together in the dense array of the sparse set. This allows to iterate all *C* over a contiguous array, akin to a `std::vector<C>`.

[Boost PolyCollection](https://github.com/boostorg/poly_collection) is a library providing three main containers. Among them, base_collection allows to store contiguously all instances of a same concrete type, provided that it derives from the base class used to initialize the base_collection. For example `base_collection<C>`, on can store instances of class inheriting from `C`, e.g., `C1, C2,... CN` contiguously, akin to `std::vector<C1>, ...std::vector<CN> `

