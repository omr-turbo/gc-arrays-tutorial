# GC API Reference

## Core API

### Object-Oriented Allocators

* Defined in: `<OMR/GC/Allocator.hpp>`
* Namespace:  `OMR::GC`

```c++
/// Allocate a managed object.
template <typename T = void, typename Init>
T* allocate(Context& cx, std::size_t size, Init&& init) noexcept;

/// Allocate a managed object, where the memory has not been initialized to zero.
/// Used when the object has little or no reference slots that need to be cleared.
template <typename T = void, typename Init>
T* allocateNonZero(Context& cx, std::size_t size, Init&& init) noexcept;

// 3
template <typename T = void, typename Init>
T* allocateNoCollect(Context& cx, std::size_t size, Init&& init) noexcept;

// 4
template <typename T = void, typename Init>
T* allocateNonZeroNoCollect(Context& cx, std::size_t size, Init&& init) noexcept;
```

* `size` is the size of the allocation, in bytes.
* `init` is a primitive initializer. The purpose of `init` is to put a newly allocated object into a GC-safe state. This could mean storing a class pointer, storing a length, or zeroing reference slots. `init` may not allocate, and cannot fail. Init is a function or function-like object, callable on `T* target`.

The return value is the address of the newly allocated object, or `nullptr` if the collector failed to satisfy the allocation.

Every call to allocate could potentially collect, which will invalidate every natively-held reference into the heap. Do not hold GC references in native variables across an allocation site. If you need to hold a reference, consider using a `StackRoot<T>`.

In a concurrent collection, every allocation is "taxed" with a certain amount of GC work. In case the object is encountered by the collector, it must be initialized and cleared to a scannable state. This is why the object must be "primitive-initialized" during the allocate call.

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

### OMR::GC::StackRoot

* Defined in; `<OMR/GC/StackRoot.hpp>`
* Namespace: `OMR::GC`

A rooted GC pointer to an instance of `T`. The target/referent must be a managed allocation. This class must be
stack allocated. StackRoots push themselves onto the linked list of stack roots at construction. When A GC occurs, the sequence of StackRoots will be walked, the referents marked, and the references fixed up. StackRoots have a strict LIFO ordering. As such, StackRoots cannot be copied or moved.

```c++
template<typename T = void>
class StackRoot
{
public:
	/// StackRoots must be stack allocated, so the new operator is explicitly removed.
	void* operator new(std::size_t) = delete;

	/// Create a new StackRoot associated with cx.
	explicit StackRoot(Context& cx, T* ref = nullptr) noexcept;

	/// Detach this StackRoot.
	~StackRoot() noexcept;

	/// Assign this StackRoot a GC reference.
	StackRoot& operator=(T* p) noexcept;

	/// The StackRoot has pointer operators if T is not void.
	T* operator->() const noexcept;
	T& operator*() const noexcept;

	/// Get a raw pointer to the target.
	T* get() const noexcept;

	/// Comparison of the referent.
	constexpr bool operator==(const StackRoot<T>& rhs) const noexcept;
	constexpr bool operator==(T* ptr) const noexcept;
	constexpr bool operator!=(const StackRoot<T>& rhs) const noexcept;
	constexpr bool operator!=(T* ptr) const noexcept;

	/// The StackRoot freely degrades into a Handle.
	operator Handle<T>() const;

	/// The StackRoot degrades into a raw pointer.
	operator T*() const;
};

template<typename T>
constexpr bool
operator==(std::nullptr_t, const StackRoot<T>& root);

template<typename T>
constexpr bool
operator!=(std::nullptr_t, const StackRoot<T>& root);
```

### The SlotHandle concept

A slot handle is a reference to a field in an object. SlotHandles are used by the collector to read and write from objects. SlotHandles allow OMR clients to implementing their own reference encoding schemes.

The minimum API a SlotHandle must implement is:
```c++
class SlotHandleT
{
public:
	/// Convert the slot to an address. Used for certain metrics in the collector.
	void *toAddress() const noexcept;

	/// Read a reference from the target slot.
	omrobjectptr_t readReference() const noexcept;

	/// Store a reference to the target slot.
	void writeReference(omrobjectptr_t value) const noexcept;

	/// Atomic-store a reference to the target slot.
	void atomicWriteReference(omrobjectptr_t value) const noexcept;
};
```

### OMR::GC::RefSlotHandle

* Defined in: `<OMR/GC/RefSlotHandle.hpp>`
* Namespace: `OMR::GC`

A handle to a slot containing a gc-reference. A implementation of the SlotHandle concept. The `RefSlotHandle` is a kind of default handle type provided by OMR, for handling plain reference slots in objects. The target slot contains an unencoded, full-width, absolute gc-reference. 

```c++
class RefSlotHandle
{
public:
	RefSlotHandle(omrobjectptr_t *slot);

	void *toAddress() const noexcept;

	omrobjectptr_t readReference() const noexcept;

	void writeReference(omrobjectptr_t value) const noexcept;

	void atomicWriteReference(omrobjectptr_t value) const noexcept;
};
```

### Read and Write Barriers

* Defined in: `<OMR/GC/Barriers.hpp>`
* namespace: `OMR::GC`

Barriered load-reference and store-references. By using barriered mutator operations, the garbage collector is able to track how the object graph changes during program execution. This is essential for generational and concurrent garbage collection.

```c++
/// Store a reference to a slot. Barriered.
template <typename SlotHandleT, typename ValueT>
void
store(Context &cx, omrobjectptr_t object, SlotHandleT slot, ValueT *value);

/// Load a reference from a slot. Barriered.
template <typename SlotHandleT, typename ValueT>
ValueT
load(Context &cx, omrobjectptr_t object, SlotHandleT slot);
```

## Object Scanning

### OMR::GC::ScanResult

* Defined in: `<OMR/GC/ScanResult.hpp>`
* Namespace: `OMR::GC`

A simple POD-type value indicating the status of an object-scan operation.

If the scan completed and there are no more references in the.

Bytes scanned
```c++
struct ScanResult
{
	std::size_t bytesScanned;
	bool complete;
};
```

### OMRClient::GC::ObjectScanner (Client Code)

* Defined in: `<OMRClient/GC/ObjectScanner.hpp>`
* Namespace `OMRClient::GC`


The object scanner is the primary interface between the collector and your objects. The scanner notifies the GC about where references are in objects, and how they are encoded.

The scanner's job is to give gc-visitors a handle to each reference-slot in the target object. The handle gives visitors a way to read or write from object-slots. There are many GC operations which need to read or write to objects in some way.

The OMR GC implements a variety of visitors to perform tasks on objects. As an example, during a global marking phase, the `MarkingVisitor` will read object's reference slots to find which objects are reachable.

The visitor's `edge` member-function returns a bool. If the result is true, scanning can continue normally. However, if the visitor returns `false`, The scanner should pause. This might happen if the collector is performing an incremental or concurrent task that needs to be interrupted early. If the scan is paused, the collector may resume scanning the current target by calling `ArrayScanner::resume`. It is the scanner's responsibility to track the target and current scan state.

The scanner's result is an `OMR::GC::ScanResult`. It is the scanner's responsibility to track how many bytes were scanned, and report that amount in the result. If the scan was interrupted before the object was completely scanned, `result.complete` must be false.

The scanner's start and resume API both take a scan budget. If the number of bytes scanned reaches (or exceeds) this budget, the scanner should save it's state and return early. Since the scan was interrupted, the returned ScanResult should indicate that `result.complete` is `false`.

```c++
class ObjectScanner {
public:
	// The scanner must be copyable
	ObjectScanner(const ObjectScanner&) = default;

	// begin scanning an object
	template <typename VisitorT>
	OMR::GC::ScanResult
	start(VisitorT&& visitor, omrobjectptr_t target, std::size_t todo = SIZE_MAX) noexcept;

	// resume scanning an object
	template <typename VisitorT>
	OMR::GC::ScanResult
	resume(VisitorT&& visitor, std::size_t todo = SIZE_MAX) noexcept;
};
```

The Object scanner is created through the `ObjectModelDelegate`, which enables the client to pass runtime configuration to the scanner at instantiation.

### Object Visitor Concept

```c++
class Visitor {
public:
	template <typename SlotHandle>
	bool edge(void* object, SlotHandle&& handle) noexcept;
};
```

The edge is called by the client's object scanner to notify the visitor of a reference edge between two objects. The handle is used to read or write to the slot containing the reference.

If the edge call returns false, the visitor should pause scanning the target. If edge returns true, scanning may continue normally. Edge call typically only returns false if the GC is performing an incremental or concurrent operation on an object.

There are a wide variety of visitors in the garbage collector.
