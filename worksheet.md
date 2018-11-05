# Splash 2018 Worksheet

In this workshop, you'll be defining simple array-like data structures, and using the OMR GC to automatically manage their memory for you.

0. Build the project
1. Implement the array structures      (30 minutes)
	1. ArrayHeader
	1. BinArray
	2. ObjectRef / AnyArray*
	3. RefArray
	4. AnyArray
2. Implement ArrayScanner              (30 minutes)
3. Implement array allocators          (15 minutes)
4. Implement and run the GC benchmark  (15 minutes)
5. Enable heap compaction              (05 minutes)
6. Enable the scavenger                (10 minutes)

total expected time: ~2 hours

## Task -1: Prerequisites and setup (15 minutes)

You will need:
- A laptop (linux, windows, or osx)
- a C++ 11 toolchain (gcc, clang, or msvc)
- cmake 3.11+, and a supported backend build system
- a debugger (gdb, windbg, or lldb)
- git

Before the tutorial you should have cloned this skeleton project:

```sh
git clone --recursive https://github.com/omr-turbo/gc-arrays-tutorial.git
```

Don't forget your laptop charger! You will be running compilations!

## Project structure

| Directory             | Contents                             |
|-----------------------|--------------------------------------|
| `include`             | All headers                          |
| `include/OMRClient/`  | Headers implemented by the OMRClient |
| `include/Splash/`     | Headers implemented for splash       |
| `omr/`                | The OMR source code (git submodule)  |
| `glue/`               | Secondary OMR/Client bindings        |
| `build_debug/`        | Debug build output directory         |
| `build_release/`      | Release build output directory       |

Notable Files:

| File                        | Contents                       |
|-----------------------------|--------------------------------|
| `api-reference.md`          | A mini-reference of GC APIs    |
| `main.cpp`                  | Benchmark code                 |
| `include/Splash/Arrays.hpp` | The Core arrays header         |

## Task 0: Building the project

The build directories haven't been created yet. First, create and initialize the debug build directory:

```bash
# from the project root
mkdir build_debug
cd build_debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

When you're ready to build your code, you can do so with these commands:

```bash
# from the project root
cd build_debug
cmake --build .
```

Create a second build directory for compiling the project in release mode:

```bash
# from the project root
mkdir build_release
cd build_release
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

Debug build artifacts are easier to debug interactively, but will execute slower than optimized release builds. When you first test your code, build in debug-mode. When everything is working, benchmark in release-mode.

## Task 1: Implement the array structures (45 minutes)

In this task, you will be defining the basic data-structures that make up our arrays, which we will be later garbage collecting. You will be using C++ structs to lay out the Arrays in memory. The arrays are required to be "standard layout" types.

### Object alignment

Objects on the OMR heap are aligned implicitly by rounding the size of all objects to a multiple of the alignment factor. As a minimum, OMR requires that objects must be at least 16 bytes in size, and aligned to 8 bytes. Today, our arrays will be aligned to 16 bytes. Define the constant `ALIGNMENT` in `include/Splash/Arrays.hpp`:

```c++
constexpr const std::size_t ALIGNMENT = 16;
```

### Define the Splash::ArrayHeader

The `ArrayHeader` is a fixed-length field located at the beginning of every heap-allocated object. The header contains enough information to inspect the data in an array. The GC needs every on-heap object to have a valid and fully initialized header. Without this, the collector will not be able to interpret the contents of an object--the object would appear to be corrupt. The header is implemented as 64bit (8 byte) integer, which encodes four values. We can use shifting and masks to manually encode and decode the metadata. The table below describes the encoding:

| bytes | Property               |
|------:|------------------------|
|     0 | GC metatdata           |
|     1 | Data kind (ref or bin) |
| 2 - 5 | Length (in elements)   |
| 6 - 7 | Unused                 |

The OMR GC will use the least-significant (0th) byte in the first word of an object for tracking internal metadata. It's important that we do not modify it's value.

First, we need a type that represents the kind of data in an array. Define the `Kind` enum in `include/Splash/Arrays.hpp`:

```c++
enum class Kind : std::uint8_t {
	REF, BIN
};
```

Then, define the `ArrayHeader` struct in `include/Splash/Arrays.hpp`:

```c++
/// Metadata about an Array. Must be the first field of any heap object.
///
/// Encoding:
///
/// Bytes           | Property | Size | Offset |
/// ----------------|----------|------|--------|
/// 0               | Metadata |   08 |     00 |
///   1             | Kind     |   08 |     08 |
///     2 3 4 5     | Length   |   32 |     16 |
///             6 7 | Padding  |   16 |     48 |
///
struct ArrayHeader {
	ArrayHeader(Kind k, std::uint32_t s)
		: value((std::uint64_t(s) << 16) | (std::uint64_t(k) << 8))
	{}

	/// The number of elements in this Array. Elements may be bytes or references.
	/// NOT the total size in bytes.
	std::uint32_t length() const {
		return std::uint32_t((value >> 16) & 0xFFFFFFFF);
	}

	/// The kind of Array this is: either a RefArray or BinArray
	Kind kind() const {
		return Kind((value >> 8) & 0xFF);
	}

	std::uint64_t value;
};
```

### Define the Splash::BinArray

As noted above, the `BinArray` must have a header as it's first field. The constructor will initialize the header with the correct type and length. When we create a `BinArray`, we will "overallocate" the struct, leaving additional memory past the end for actual data storage. An unknown-length `data` array can be used to index into this trailing storage.

Now define the `BinArray` struct in `include/Splash/Arrays.hpp`:

```c++
// namespace Splash
struct BinArray {
	BinArray(std::uint32_t nbytes)
		: header(Kind::BIN, nbytes), data() {}
 
	ArrayHeader header;
	std::uint8_t data[];
};
```

The actual size of the array is calculated by adding the fixed size of the header, to the variable size of the data, and rounding for alignment. Define the `binArraySize` helper in `include/Splash/Arrays.hpp`:

```c++
// namespace Splash
constexpr std::size_t binArraySize(std::uint32_t nbytes) {
	return align(sizeof(BinArray) + nbytes, ALIGNMENT);
}
```

### Define the GC-Reference type `AnyArray*`, aka `OMRClient::GC::ObjectRef`

The `OMRClient::GC::ObjectRef` is used in OMR as the fundamental gc-reference type, and is our first piece of OMR client code.

In this object model, a reference is a pointer to either a `RefArray` or a `BinArray`. If we define `AnyArray` as a union of our two array types, then an `AnyArray*` can be used to signify that a pointer could refer to _either_ a `RefArray` or a `BinArray`. `AnyArray` is only ever used as a pointer-type, and is never be actually instantiated.

You will be defining the union later, but for now, forward declare the `AnyArray` union in `include/OMRClient/GC/ObjectRef.hpp`:

``` c++
namespace Splash {
union AnyArray;
}
```

Now, alias the `ObjectRef` typename to `Splash::AnyArray`:

```c++
// namespace OMRClient::GC
using ObjectRef = Splash::AnyArray*;
```

### Define the Splash::RefArray

Now that we've defined what a reference is, we can define `RefArray`.

The `RefArray` is layed out very similarly to the `BinArray`. Again, the first field must be the `ArrayHeader`, and storage for the references will be allocated past the end of the struct. The `RefArray` will store elements of type `Ref`, an alias to `AnyArray*`. The RefArray has some additional member functions which will come in handy when we need to scan the array's references. Define the `RefArray` struct in `include/Splash/Arrays.hpp`:

```c++
// namespace Splash

using RefSlot = AnyArray*;

struct RefArray {
	RefArray(std::uint32_t nrefs)
		: header(Kind::REF, nrefs), data() {}

	std::uint32_t length() const { return header.length(); }

	/// Returns a pointer to the first slot.
	RefSlot* begin() { return &data[0]; }

	/// Returns a pointer "one-past-the-end" of the slots.
	RefSlot* end() { return &data[length()]; }

	ArrayHeader header;
	RefSlot data[];
};
```

Define the `refArraySize` helper function in `include/Splash/Arrays.hpp`:

```c++
// namespace Splash
constexpr std::size_t refArraySize(std::uint32_t nrefs) {
	return align(sizeof(RefArray) + (sizeof(RefSlot) * nrefs), ALIGNMENT);
}
```

### Define the AnyArray union

You have already forward-declared the `AnyArray` union above. Now that the `RefArray` and `BinArray` types have been defined, you can write the actual definition. Define the `AnyArray` union in `include/Splash/Arrays.hpp`:

```c++
// namespace Splash
union AnyArray {
	// AnyArray can not be constructed
	AnyArray() = delete;

	ArrayHeader asHeader;
	RefArray asRefArray;
	BinArray asBinArray;
};
```

To prevent users from instantiating the `AnyArray`, the constructor is explicitly deleted. Remember, `AnyArray` may only ever be used as a pointer-type.

Both the `RefArray` and `BinArray` have valid headers as their first field, which allows us to access the header of an `AnyArray` directly, regardless of what the underlying type actually is. The helper function `kind` will return the type of an `AnyArray` by reading the header. Define `kind` in `include/Splash/Arrays.hpp`:

```c++
// namespace Splash

/// Find the kind of array by reading from it's header.
inline Kind kind(AnyArray* any) {
	return any->asHeader.kind();
}
```

The function `size` will read an array's header to find it's total size, regardless of type. Implement the `size` function in `include/Splash/Arrays.hpp`:

```c++
// namespace Splash

/// Get the total allocation size of an array, in bytes.
inline std::size_t size(AnyArray* any) {
	std::size_t sz = 0;
	switch(kind(any)) {
	case Kind::REF:
		sz = refArraySize(any->asHeader.length());
		break;
	case Kind::BIN:
		sz = binArraySize(any->asHeader.length());
		break;
	default:
		// unrecognized data!
		assert(0);
		break;
	}
	return sz;
}
```

### Implement the "object size" functionality in `OMRClient::GC::ObjectModelDelegate`

This is our second piece of client code. The `ObjectModel` gives the collector basic information about manged objects. You need to implement the object-size APIs. The object-size must be aligned! In `include/OMRClient/GC/ObjectModelDelegate.hpp`, implement the following member-functions for `ObjectModelDelegate`:

1. `getObjectHeaderSizeInBytes`:
```c++
MMINLINE uintptr_t
getObjectHeaderSizeInBytes(Splash::AnyArray* any)
{
	return sizeof(Splash::ArrayHeader);
}
```

2. `getObjectSizeInBytesWithHeader`:
```c++
MMINLINE uintptr_t
getObjectSizeInBytesWithHeader(Splash::AnyArray* any)
{
	return Splash::size(any);
}
```

## Task 2: Implement the `Splash::ArrayScanner` (30 minutes)

Now that you've defined the structure of the array types, and implemented the object size and object-reference client code, it's time to teach the collector how to scan the arrays. Your task is to implement an `ArrayScanner`, which we will later bind to the `OMRClient::GC::ObjectScanner` type. The [api-reference](./api-reference.md) documents the requirements of an [`ObjectScanner`](./api-reference.md#object-scanning), as well as the notion of a [`SlotHandle`](./api-reference.md#the-slothandle-concept). Implementing the scanner can be surprisingly challenging. Luckily, we only have to do it once.

The object scanner needs to be able to scan any type of array, even though the `BinArray` has no references. The scanner will give the visitor a handle to each reference-slot in the `RefArray`. You can use the basic `OMR::GC::RefSlotHandle` type. The `RefSlotHandle` is a handle to a slot containing a plain (unencoded) `OMRClient::GC::ObjectRef`. Think of it like a `RefSlot` pointer. It's possible to use a custom slot-handle type to implement your own reference encoding scheme.

Implement the `ArrayScanner` in `include/Splash/ArrayScanner.hpp`:

```c++
// namespace Splash

class ArrayScanner {
public:
	ArrayScanner() = default;

	ArrayScanner(const ArrayScanner&) = default;

	template <typename VisitorT>
	OMR::GC::ScanResult
	start(VisitorT&& visitor, AnyArray* any, std::size_t bytesToScan = SIZE_MAX) {
		target_ = any;
		switch(kind(any)) {
		case Kind::REF:
			return startRefArray(std::forward<VisitorT>(visitor), bytesToScan);
		case Kind::BIN:
			// no references to scan
			return {0, true};
		default:
			// uh-oh: corrupt heap!
			assert(0);
			return {0, true};
		}
	}

	template <typename VisitorT>
	OMR::GC::ScanResult
	resume(VisitorT&& visitor, std::size_t bytesToScan = SIZE_MAX) {
		switch(kind(target_)) {
		case Kind::REF:
			return resumeRefArray(std::forward<VisitorT>(visitor), bytesToScan);
		case Kind::BIN:
			// uh-oh: should never resume a BinArray
			assert(0);
			return {0, true};
		default:
			// uh-oh: corrupt heap!
			assert(0);
			return {0, true};
		}
	}

private:
	template <typename VisitorT>
	OMR::GC::ScanResult
	startRefArray(VisitorT&& visitor, std::size_t bytesToScan) {
		current_ = target_->asRefArray.begin();
		return resumeRefArray(std::forward<VisitorT>(visitor), bytesToScan);
	}

	template <typename VisitorT>
	OMR::GC::ScanResult
	resumeRefArray(VisitorT&& visitor, std::size_t bytesToScan) {
		RefSlot* end = target_->asRefArray.end();

		assert(current_ <= end);

		bool cont = true;
		std::size_t bytesScanned = 0;

		while (true) {
			if (current_ == end) {
				// object complete
				return {bytesScanned, true};
			}
			if (bytesScanned >= bytesToScan || !cont) {
				// hit scan budget or paused by visitor
				return {bytesScanned, false};
			}

			if (*current_ != nullptr) {
				cont = visitor.edge(target_, OMR::GC::RefSlotHandle(current_));
			}

			current_ += 1;
			bytesScanned += sizeof(RefSlot);
		}

		// unreachable
	}

	AnyArray* target_;
	RefSlot* current_;
};
```

In the client code, The `ObjectScanner` type is already bound to a "no-operation" object scanner, which does just enough to satisfy the compiler. Rewrite the alias to bind to your own `ArrayScanner` in `include/OMRClient/GC/ObjectScanner.hpp`:

```c++
// namespace OMRClient::GC
using ObjectScanner = Splash::ArrayScanner;
```

## Task 3: Implement the Array allocators (15 minutes)

We've finally fully defined our array objects, and we're ready to start allocating. Use the OMR GC's [object-allocators](./api-reference.md#object-oriented-allocators) to instantiate new array objects on the heap. The [api-reference](./api-reference.md) documents these calls.

Each allocator takes a "primitive initializer" function (or function-like object). The initializer's responsibility is to clear a newly-allocated object's memory, putting it into a consistent state for the collector to scan. The initializer function may not allocate, and may not fail. From an "application" perspective, the object may not yet be fully initialized.

Every allocation could potentially do some garbage collection work. This is why even objects currently being allocated must be in a scannable state--the GC may attempt to scan it before allocate returns.

Implement the "primitive initializer" for `BinArray` in `include/Splash/Allocators.hpp`:

```c++
// namespace Splash

/// A function-like object for initializing BinArray allocations
class InitBinArray {
public:
	/// Construct an initializer for a BinArray with nbytes of data
	InitBinArray(std::size_t nbytes) : nbytes_(nbytes) {}

	/// InitBinArray is callable like a function
	void operator()(BinArray* target) {
		// Use placement-new to initialize the target with the BinArray constructor.
		new(target) BinArray(nbytes_);
	}

private:
	std::size_t nbytes_;
};
```

InitBinArray will install the new BinArray's header, and nothing else.

Implement the `BinArray` allocator in `include/Splash/Allocators.hpp`:

```c++
// namespace Splash
inline BinArray* allocateBinArray(OMR::GC::Context& cx, std::size_t nbytes) {
	return OMR::GC::allocateNonZero<BinArray>(cx, binArraySize(nbytes), InitBinArray(nbytes));
}
```

The allocator does not need to zero the new array's data, because a BinArray's slots are not scanned by the collector. We can use `OMR::GC::allocateNonZero` to allocate from non-zero-initialized memory, which makes allocating BinArrays faster.

### Implement the RefArray allocator

Implement the primitive-initializer for `RefArray` in `include/Splash/Allocators.hpp`:

```c++
// namespace Splash
class InitRefArray {
public:
	InitRefArray(std::size_t nrefs) : nrefs_(nrefs) {}

	void operator()(RefArray* target) {
		new (target) RefArray(nrefs_);
	}

	std::size_t nrefs_;
};
```

Again, the initializer is just creating the header. However, the new array will need it's reference-slots cleared, in order to be safely walkable. Rather than zero the slots in the initializer, we can allocate the object from pre-zeroed memory. By using the plain `allocate` call, OMR will make the allocation out of zero-initialized memory.

Implement the `RefArray` allocator in `include/Splash/Allocators.hpp`:

```c++
// namespace Splash
inline RefArray* allocateRefArray(OMR::GC::Context& cx, std::size_t nrefs) {
	return OMR::GC::allocate<RefArray>(cx, refArraySize(nrefs), InitRefArray(nrefs));
}
```
This ensures that the references will be zeroed, and the new array is safe to walk.

## Task 4: Implement and run the GC benchmark (15 minutes)

Our benchmark today is extremely simple:
1. Allocate a single `RefArray` with 1000 elements. This is our one root object.
2. In a loop: allocate `BinArrays` of varying size, and store into the root at varying indexes.

The parent `RefArray` is rooted using an [`OMR::GC::StackRoot<T>`](./api-reference.md#omr::gc::stackroot)
Effectively, this toy benchmark is measuring the cost of allocating and freeing objects. This is not representative of how a real application behaves. Regardless, it can still be an interesting exercise in measuring the minor overhead of malloc versus a GC.

A version testing malloc is already implemented. Implement the GC benchmark in `main.cpp`:

```c++
void gc_bench(OMR::GC::RunContext& cx) {
	OMR::GC::StackRoot<Splash::RefArray> root(cx);
	root = Splash::allocateRefArray(cx, ROOT_SIZE);
	for (std::size_t i = 0; i < ITERATIONS; ++i) {
		// be careful to allocate the child _before_ dereferencing the root.
		auto child = (Splash::AnyArray*)Splash::allocateBinArray(cx, childSize(i));
		root->data[index(i)] = child;
	}
}
```

Compile the project in debug mode, and run the benchmark:

```bash
# in the project root
cd build_debug
cmake --build .
./main
```

The collector can output verbose logs, which you can inspect to see what GC operations are taking place. You should see a large number of global collections due to allocation failures or TLH refresh failures. Run the benchmark with verbose logs enabled:

```bash
# in build_debug/
export OMR_GC_OPTIONS="-Xverbosegclog:stderr"
./main
```

 Remember, OMR has been built without optimizations, so you can expect gc performance to be much worse than malloc. Compile the project in release mode, and run the benchmark again. Run without verbose logging, which will affect the GC's throughput.

```bash
# in the project root
cd build_release
cmake --build .
export OMR_GC_OPTIONS=""
./main
```

How do the times compare?

### Optional: Implement a printing visitor

You might want to implement a visitor that prints the references in a `RefArray`. The visitor could be used to validate your object scanner. You can put this visitor before `gc_bench` in `main.cpp`:

```c++
// in main.cpp, no namespace:

class PrintingVisitor {
public:
	explicit PrintingVisitor() {
	}

	/// Print data about each edge
	template <typename SlotHandleT>
	bool edge(void* object, SlotHandleT slot) {
		std::cerr << "  (edge" 
		          << " :slot "  << slot.toAddress()
		          << " :child " << slot.readReference()
		          << ")\n";
		return true; // always continue
	}
};
```

To apply the printing visitor to an array, pass the `PrintingVisitor` the scanner you defined above. In `main.cpp`, define the helper `printReferencesInObjects`:

```c++
// in main.cpp, no namespace:

inline void printReferencesInObject(Splash::AnyArray* target) {
	std::cerr << "(object " << target;
	Splash::ArrayScanner scanner;
	OMR::GC::ScanResult result = scanner.start(PrintingVisitor(), target);
	assert(result.complete); // should never pause
	std::cerr << ")" << std::endl;
}
```

If you're having crashes, you might use this visitor at the start of every iteration, to print the slots in the root `RefArray`, and manually ensure every reference is found correctly. Remember, a RefArray won't have any children, until you add them, and a BinArray never has any references.

You can call the print helper like so:

```c++
printReferencesInObject((Splash::AnyArray*)root.get());
```

## Task 5: Enable heap compaction (5 minutes)

As an application executes, objects will be allocated and collected dynamically. As objects become unused, these spaces become "holes" of free memory in the heap. Sometimes, these holes are too small to be reused, and live on as fragments of unusable memory. Heap fragmentation is especially problematic for long-running processes: Fragmentation results in:

1. Increased total memory consumption (holes are a waste of "in use" memory)
2. Slower applications, by hurting cache locality
3. Slower allocations, by forcing allocators to search for free space more often
4. Unexpected out-of-memory conditions, when free memory can't be reused

To combat heap fragmentation, the collector can perform a sliding compaction of live objects, grouping objects together, leaving a contiguous span of free space at the end of the heap.


In `OmrConfig.cmake`, enable the compaction compile flag:

```cmake
set(OMR_GC_MODRON_COMPACTION ON  CACHE INTERNAL "")
```

Rebuild the project in debug mode, and enable the compactor and verbose-log runtime option:

```bash
# in the project root
cd build_debug
cmake --build .
export OMR_GC_OPTIONS="-Xcompactgc -Xverbosegclog:stderr"
./main
```

You should be able to see the compactor sliding 1001 objects at each global collection. Run the benchmark with the compactor enabled, in release mode:

```bash
# in the project root
cd build_release
cmake --build .
export OMR_GC_OPTIONS="-Xcompactgc"
./main
```

How does the compactor affect our benchmark time?

## Task 5: Enable the scavenger (20 minutes)

In OMR, the scavenger is our semispace-copying generational collector. This task is to use the scavenger to improve GC time. The scavenger is a copying collector,

Turn on the scavenger compile flag in `OmrConfig.cmake`:

```cmake
set(OMR_GC_MODRON_SCAVENGER  ON CACHE INTERNAL "")
```

### Teach the scavenger to read the size of objects from the header

The scavenger caches the first word of an object when moving--this is our ArrayHeader.
The word is embedded in the `ForwardedHeader`, a special on-heap structure that contains the new address of an object. In some cases, the scavenger will need to determine the size of an object from it's preserved header slot.

Implement the `ObjectModelDelegate` member-function `getForwardedObjectSizeInBytes` in `OMRClient/GC/ObjectModelDelegate.hpp`:

```c++
MMINLINE uintptr_t
getForwardedObjectSizeInBytes(MM_ForwardedHeader *forwardedHeader)
{
  // Construct a temporary array header on the stack by copying the preserved heap slot.
  // Use the on-stack header to determine the original object's size.
  Splash::ArrayHeader header((Splash::Kind)-1, 0);
  header.value = forwardedHeader->getPreservedSlot();

  /// Use splash::size to calculate the size of the object from our copy of it's header.
  return Splash::size((Splash::AnyArray*)&header);
}
```

### Barrier all reference stores

The GC needs to manage a set of remembered objects in old space. We must use a write barrier every time a reference is stored to an object. This will allow us to determine when an edge is created from old-space to new-space, and remember the origin object.

Start by implementing a helper for building reference slots handles. Define the `at` helper in `include/Splash/Barriers.hpp`:

```c++
/// Return a handle to array.data[index]
inline OMR::GC::RefSlotHandle at(RefArray& array, std::size_t index) {
  return OMR::GC::RefSlotHandle(&array.data[index]);
}
```

The `OMR::GC::store` function will write a reference to an object and trigger the required barriers. Implement a function that assigns a reference into a `RefArray` at a given index, using `OMR::GC::store`.

Implement `store` in `include/Splash/Barriers.hpp`:
```c++
/// Store a ref to array->data[index]
inline void store(OMR::GC::RunContext& cx, RefArray& array,
                  std::size_t index, RefSlot value) {
  OMR::GC::store(cx, (AnyArray*)&array, at(array, index), value);
}
```

In `main.cpp`, modify the benchmark to use the new `store` API to assign into the slots of the root RefArray:

```c++
void gc_bench(OMR::GC::RunContext& cx) {
  OMR::GC::StackRoot<Splash::RefArray> root(cx);
  root = Splash::allocateRefArray(cx, ROOT_SIZE);
  for (std::size_t i = 0; i < ITERATIONS; ++i) {
    auto child = (Splash::AnyArray*)Splash::allocateBinArray(cx, childSize(i));
    Splash::store(cx, *root, index(i), child);
  }
}
```

Note:  Do not dereference the `StackRoot` before allocating. The allocation call could collect, and cause the root object to move!

### Watch the verbose GC logs to see when the collector scavengers

The scavenger is enabled at runtime by setting the GC option `-Xgcpolicy:gencon`. Enable the scavenger and verbose logging, and run main in debug mode:

```bash
export OMR_GC_OPTIONS="-Xgcpolicy:gencon -Xverbosegclog:stderr"
cd build
cmake --build .
./main
```

* Can you see the scavenger copying objects?
* Is the root object in the remembered set at the beginning? At the end?
* How many objects are being copied?
* Are global collections happening?

### Run the benchmark in release mode

Rebuild the project in release mode, and rerun the benchmark (with verbose disabled):

```bash
cd build_release
cmake --build .
export OMR_GC_OPTIONS="-Xgcpolicy:gencon"
./main
```

### Run with both compaction and scavenger

Now try with both the compactor and scavenger enabled:

```bash
# in build_release
export OMR_GC_OPTIONS="-Xcompactgc -Xgcpolicy:gencon"
```

You should see that since very few objects are actually tenured, the effects of the compactor are minimal in our benchmark. However, in a real application, the compactor is critical for managing memory fragmentation for long running processes, like web servers.

## You're Done!!

Thank you very much for participating in this workshop!

## Bonus Task: Implement compressed references

In this task we're going to use a smaller width datatype to represent our references. This will give our reference arrays better density. There is an issue though, we cannot represent every possible address in fewer than 64 bits.

If we chose to represent references in a 32bit property, we know that the maximum heap address we can represent is 2^32, or 4gib. However, we can play with object alignment to increase that limit dramatically on 64 bit systems.

If we know objects sizes are always a multiple of 2, then every address on the heap must be even, and thus have a zero in the least significant bit. That's one bit that's zero in every valid reference! We can shift the heap addresses right by one bit as we compress, effectively doubling the maximum heap address we can represent in 32 bits. For an object alignment 2^N, there are N free bits in the address.

The table below shows the relationship between object alignment and maximum address in a 32bit value.

Alignment (bytes) | N (free bits) | Max Heap Address (gib)
------------------|---------------|-----------------------
 1                | 0             |   4
 2                | 1             |   8
 4                | 2             |  16
 8                | 3             |  32
16                | 4             |  64
32                | 5             | 128


Today, our arrays are aligned to 16 bytes, which gives us a compression shift of 4, and a maximum heap address of 64 gib. Note that "maximum address" is not the same as "heap size"! The entirety of the heap must be located beneath the maximum address limit.

1. In `RefArray`, make the `RefSlot` type alias `OMR::GC::CompressedRef`
2. Add the constant COMPRESSED_REF_SHIFT = 4 to the top of `Arrays.hpp`
3. Have the slot handle maker (`Splash::at`) create an `OMR::GC::CompressedRefSlotHandle`, with shift 4.
4. Make the object scanner create compressed ref slot-handles.
5. Set the maximum heap address on startup--unfortunately, not yet possible with OMR.