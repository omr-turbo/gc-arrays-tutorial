# GC API Reference

## Core API

### Allocation

* defined in: `<OMR/GC/Allocator.hpp>`
* namespace:  `OMR::GC`

```c++
// 1
template <typename T = void, typename Init>
T* allocate(Context& cx, std::size_t size, Init&& init) noexcept;

// 2
template <typename T = void, typename Init>
T* allocateNonZero(Context& cx, std::size_t size, Init&& init) noexcept;

// 3
template <typename T = void, typename Init>
T* allocateNoCollect(Context& cx, std::size_t size, Init&& init) noexcept;

// 4
template <typename T = void, typename Init>
T* allocateNonZeroNoCollect(Context& cx, std::size_t size, Init&& init) noexcept;
```

1: Allocate a managed object.

2: Allocate a managed object, where the memory has not been initialized to zero. Used when the object has little or no reference slots that need to be cleared.

3 and 4: Allocate without falling back to a collection.

Parameters:

* `size` is the size of the allocation, in bytes.
* `init` is a primitive initializer. The purpose of `init` is to put a newly allocated object into a GC-safe state. This could mean storing a class pointer, storing a length, or zeroing reference slots. `init` may not allocate, and cannot fail.

The return value is the address of the newly allocated object, or `nullptr` if the collector failed to satisfy the allocation.

Every call to allocate could potentially collect. This is known as a GC-safepoint. A collection could potentially inval. This means, it is absolutely unsafe to hold GC references in native variables across an allocation site. Every native reference is invalidated by an allocation. If you need to hold a reference across gc-safepoints, consider using a `StackRoot<T>`.

In a concurrent collection, every allocation is "taxed" with a certain amount of GC work.
In case the object is encountered by the collector, it must be initialized to a scannable (if empty) state. This The object must be initialized before the call is completed. The

For finer grained control

#### Example

```c++
/// Contains metadata about the contents of the object.
struct Object {
	std::uintptr_t gcflags;
	std::size_t nslots;
	std::uintptr_t slots[0];
};

/// Calculate the size of an object from it's number of slots.
std::size_t objectSize(std::size_t nslots) {
	return sizeof(Object) + (sizeof(std::uintptr_t) * nslots);
}

/// Initialize the object's header by setting the number of slots.
/// The memory is zeroed, no need to manually clear the slots.
struct InitObject {
	void operator()(Object* target) noexcept {
		target->header = 0;
		target->size = size;
	}
	std::size_t size;
};

/// Allocate and primitive-initialize an object with n slots.
Object* allocateObject(std::size_t nslots) noxcept {
	return OMR::GC::allocate<Object>(cx, objectSize(nslots), InitObject(nslots));
}
```

### Raw Allocation

* Defined in: `<OMR/GC/Allocator.hpp>`
* namespace `OMR::GC`

```c++
// TODO
```

### StackRoot

* Defined in; `<OMR/GC/StackRoot.hpp>`
* Namespace: `OMR::GC`

```c++
template <typename T>
class StackRoot {}
```

A pointer-like class that automatically roots it's referents.
StackRoots have strict LIFO semantics, and must be stack allocated. 

#### Member Functions
Constructors:

```c++
// 1
StackRoot(Context& cx) noexcept;

// 2
StackRoot(Context& cx, T* ref) noexcept;

// 3
StackRoot(const StackRoot&) = delete;
```

1. Create a new `StackRoot<T>` attached to `cx`, with a `nullptr` referent.
2. Create a new `StackRoot<T>` attached to `cx`, rooting `ref`.
3. They cannot be moved or copied.

---

```c++
T* operator->() const noexcept;
T& operator*() const noexcept;
```

StackRoots are dereferenceable, like a pointer, unless T is void.

---

```c++
T* get() const noexcept;
```

Obtain the raw reference from the stack root.


### Read and Write Barriers

* Defined in: `<OMR/GC/Barriers.hpp>`
* namespace: `OMR::GC`

```c++
template <typename SlotHandle, typename Value>
void store(Context& cx, void* object, SlotHandle slot, Value value) noexcept;
```

Store a reference into 

```c++
template <typename SlotHandle>
void read(Context)
```

### ScanResult

```c++
struct ScanResult
{
	std::size_t bytesScanned;
	bool complete;
};
```

## Client Code

### OMRClient::GC::ObjectScanner

defined in: `<OMRClient/GC/ObjectScanner.hpp>`

```c++
class ObjectScanner {};
```

The `ObjectScanner` is the main interface between

#### Member Functions

```c++
```

## Concepts

### Object Visitor (concept)

```c++
class Visitor {};
```

#### Member Functions

```c++
template <typename SlotHandle>
bool edge(void* object, SlotHandle&& handle) noexcept;
```

Notify the visitor of a reference edge between two objects. The handle is used to read or write to the slot containing the reference.

If the edge call returns false, the visitor should pause scanning the target. If edge returns true, scanning may continue normally. Edge call typically only returns false if the GC is performing an incremental or concurrent operation on an object.

### SlotHandle (concept)

```c++
class SlotHandle {};
```

#### Member Functions

```c++
void* readReference() const noexcept;
```
* Read a reference from the slot.

---

```c++
void writeReference(void* reference) const noexcept;
```

Write a reference to the slot.

---

```c++
void* toAddress() const noexcept;
```

Convert the handle to an address. The address of the slot we are read/writing from.