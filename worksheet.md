# Splash 2018 Worksheet

In this workshop, you'll be defining simple array-like data structures, and using the OMR GC to automatically manage their memory for you.

0. Implement the array datastructures (30 minutes)
1. Implement the array scanner (30 minutes)
2. Implement the array allocators (15 minutes)
3. Implement and run the GC benchmark (15 minutes)
4. Enable heap compaction (10 minutes)
5. Enable the scavenger (20 minutes)

total expected time: 2 hours

## Task -1: Prerequisites and setup (15 minutes)

You will need:
- a C++ 11 toolchain (gcc, clang, or msvc)
- linux, windows, or osx
- git
- cmake 3.11+, and a supported backend build system

Before the tutorial you should have cloned this skeleton project:

```sh
git clone --recursive https://github.com/rwy0717/splash2018-gc-arrays
```

## Task 0: Implement the array datastructures (30 minutes)

In this task, you will be defining the basic array data-structures, which we will be later garbage collecting. You will be using C++ structs to lay out the Arrays in memory. The arrays are required to be simple structs ("standard layout" types in c++ parlance).

### The `Splash::AnyArray`, `Splash::Ref`, and `omrobjectptr_t` Types

Forward-declare the `AnyArray` union in `include/Splash/Arrays.hpp`:

```c++
union AnyArray;
```

The type `AnyArray*` is a "pointer-to an array with an unknown type". `AnyArray*` is used to signify that a pointer could refer to _either_ a `RefArray` or a `BinArray`. `AnyArray` is only ever used this way, and can never be actually instantiated. You will be defining this type further down.

Define `Ref`, an alias for `AnyArray*`, in `include/Splash/Arrays.hpp`:

```c++
using Ref = AnyArray*;
```

The `Ref` type is used to indicate a value is a "GC reference". We will be using `Ref` as our fundamental object pointer type. A ref can point to any type of array.

### Implement the object description glue

`glue/include/objectdescription.hpp` is our first set of "OMRClient bindings". This header, provided by the client, defines a set of pointer data types. These are the fundamental types that the OMR GC will use internally, when referring to objects.

(glue-code is an older name for client-code)

In `glue/include/objectdescription.hpp`, add the following code:

```c++
namespace Splash {
	union AnyArray;
}

typedef Splash::AnyArray* languageobjectptr_t;  // object reference, used by langauge
typedef Splash::AnyArray* omrobjectptr_t;       // object reference, used by OMR internally
typedef Splash::AnyArray* omrarrayptr_t;        // array reference, used by OMR internally
typedef uintptr_t fomrobject_t;                 // object-reference field in object
typedef uintptr_t fomrarray_t;                  // array-reference field in object or array
```

Some clients will make more exotic definitions, but for us, using a `Splash::Ref` (aka `Splash::AnyArray*`) everywhere is sufficient.

### The `Splash::ArrayHeader`

The `ArrayHeader` is a fixed-length field located at the beginning of every heap-allocated object. The header contains enough information to inspect an array:
1. The array kind
2. The array's length (in elements)
3. The GC metadata

The GC needs every on-heap object to have a valid and fully initialized header. Without this, the collector will not be able to interpret the contents of an object--the object would appear to be corrupt. The header is implemented as 64bit (8 byte) integer, which encodes four values. We can use shifting and masks to manually encode and decode the metadata. The table below describes the encoding:

| bytes | Property          |
|------:|-------------------|
|     0 | GC metatdata      |
|     1 | Kind              |
| 2 - 5 | Length            |
| 6 - 7 | Unused            |

First, we need a type that represents the type of an array. Define the `Kind` enum in `include/Splash/Arrays.hpp`:

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
///     2 3 4 5     | Size     |   32 |     16 |
///             6 7 | Padding  |   16 |     48 |
///
struct ArrayHeader {
	constexpr ArrayHeader(Kind k, std::uint32_t s) noexcept
		: value((std::uint64_t(s) << 16) | (std::uint64_t(k) << 8))
	{}

	/// The number of elements in this Array. Elements may be bytes or references.
	/// NOT the total size in bytes.
	constexpr std::uint32_t length() const {
		return std::uint32_t((value >> 16) & 0xFFFFFFFF);
	}

	/// The kind of Array this is: either a RefArray or BinArray
	 constexpr Kind kind() const {
		return Kind((value >> 8) & 0xFF);
	}

	std::uint64_t value;
};
```

The OMR GC uses the least-significant byte in the first word of an object for tracking internal metadata.

### The `Splash::BinArray` struct

Now define the `BinArray` struct in `include/Splash/Arrays.hpp`:

```c++
struct BinArray {
	constexpr BinArray(std::uint32_t nbytes)
		: header(Kind::BIN, nbytes), data() {}
 
	ArrayHeader header;
	std::uint8_t data[0];
};
```

As noted above, the `BinArray` must have a header as it's first field. The constructor is used to initialize the header with the correct type and length.

When we create a BinArray, we will "overallocate" the struct, leaving additional memory past the end for actual data storage. The zero-length `data` field can be used to index into this trailing storage.

Define the `binArraySize` helper in `include/Splash/Arrays.hpp`:

```c++
constexpr std::size_t binArraySize(std::uint32_t nbytes) {
	return align(sizeof(BinArray) + nbytes, 16);
}
```

The actual size of the array is calculated by adding the fixed size of the header, to the variable size of the data. Our arrays are aligned to 16 bytes, by forcing every object size to be a multiple of 16.

### The `Splash::RefArray` struct

The `RefArray` is layed out very similarly to the `BinArray`. Again, the first element must be the `ArrayHeader`, and storage for the references will be allocated past the end of the struct. The RefArray has some additional member functions which will come in handy when we need to scan the array's references.

Define the `RefArray` struct in `include/Splash/Arrays.hpp`:

```c++
struct RefArray {
	constexpr RefArray(std::uint32_t nrefs)
		: header(Kind::REF, nrefs), data() {}

	constexpr std::uint32_t length() const { return header.length(); }

	/// Returns a pointer to the first slot.
	Ref* begin() { return &data[0]; } 

	/// Returns a pointer "one-past-the-end" of the slots.
	Ref* end() { return &data[length()]; }

	ArrayHeader header;
	Ref data[0];
};
```

Define the `refArraySize` helper function in `include/Splash/Arrays.hpp`:

```c++
constexpr std::size_t refArraySize(std::uint32_t nrefs) {
	return align(sizeof(RefArray) + (sizeof(Ref) * nrefs), 16);
}
```

### Define the AnyArray union

You have already forward-declared the `AnyArray` union above. Now that the `RefArray` and `BinArray` types have been defined, you can write the actual definition.

Define the `AnyArray` union in `include/Splash/Arrays.hpp`:

```c++
union AnyArray {
	// AnyArray can not be constructed
	AnyArray() = delete;

	ArrayHeader asHeader;
	RefArray asRefArray;
	BinArray asBinArray;
};
```

To prevent users from instantiating the `AnyArray`, the constructor is explicitly deleted. Remember, `AnyArray` may only ever be used as a pointer-type.

Both the `RefArray` and `BinArray` have valid headers as their first field, which allows us to access the header of an `AnyArray` directly, regardless of what the underlying type actually is.

The helper function `kind` will returns the type of an `AnyArray` by reading the header. Define the function `kind` in `include/Splash/Arrays.hpp`:

```c++
/// Find the kind of array by reading from it's header.
constexpr Kind kind(AnyArray* any) {
	return any->asHeader.kind();
}
```

The helper function `size` will read an array's header to find it's total size, regardless of type. Implement the `size` function in `include/Splash/Arrays.hpp`:

```c++
/// Get the total allocation size of an array, in bytes.
inline std::size_t size(AnyArray* any) noexcept {
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

### Implement the ObjectModel

The object model is an older glue API the GC relies on for answering questions about objects, such as object size. You need to implement some missing functionality in the object model.

In `glue/include/ObjectModelDelegate.hpp`, implement the following member-functions for `MM_ObjectModelDelegate`:

1. `getObjectHeaderSizeInBytes`:
```c++
MMINLINE uintptr_t
getObjectHeaderSizeInBytes(omrobjectptr_t objectPtr)
{
	return sizeof(Splash::ArrayHeader);
}
```

2. `getObjectSizeInBytesWithHeader`:

```c++
MMINLINE uintptr_t
getObjectSizeInBytesWithHeader(omrobjectptr_t objectPtr)
{
	return Splash::size(objectPtr);
}
```

## Task 1: Implement the `ArrayScanner` (30 minutes)

The RefArray and BinArray types are already implemented. Your task is to implement and test an ArrayScanner class. Implementing the scanner can be surprisingly challenging. Luckily, we only have to do it once.

Check out the api-reference to see how the ObjectScanner interface is intended to work.

The most basic slot-handle is the `OMR::GC::RefSlotHandle`, that is, a handle to a slot containing a plain (unencoded) reference. Think of it like a `void**`.  This is the slot handle type you'll be using today. It's possible to use a custom slot-handle type to implement your own reference encoding.

Implement the `ArrayScanner` in `include/ArrayScanner.hpp`:

```c++
class ArrayScanner {
public:
	ArrayScanner() = default;

	ArrayScanner(const ArrayScanner&) = default;

	template <typename VisitorT>
	OMR::GC::ScanResult
	start(VisitorT&& visitor, AnyArray* any, std::size_t bytesToScan = SIZE_MAX) noexcept {
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
	resume(VisitorT&& visitor, std::size_t bytesToScan = SIZE_MAX) noexcept {
		switch(kind(target_)) {
		case Kind::REF:
			return resumeRefArray(std::forward<VisitorT>(visitor), bytesToScan);
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
		Ref* end = target_->asRefArray.end(); 

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
			bytesScanned += sizeof(Ref);
		}

		// unreachable
	}

	AnyArray* target_;
	Ref* current_;
};

```

Add the binding alias to the client code in `include/OMRClient/ObjectScanner.hpp`:

```c++
using ObjectScanner = ::Splash::ArrayScanner;
```

## Task 2: Implement the Array allocators (time 15 minutes)

Use the OMR GC's object-allocators to instantiate new array objects on the heap. The [api-reference](./api-reference.md) documents these calls.

Each allocator takes a "primitive initializer" function (or function-like object). The initializer's responsibility is to clear a newly-allocated object's memory, putting it into a consistent state for the collector to scan. The initializer function may not allocate, and may not fail. From an "application" perspective, the object may not yet be fully initialized.

Every allocation could potentially do some garbage collection work. This is why even objects currently being allocated must be in a scannable state--the GC may attempt to scan it before allocate returns.

Implement the "primitive initializer" for `BinArray` in `include/Splash/Allocators.hpp`:

```c++
// A function-like object for initializing BinArray allocations
class InitBinArray {
public:
	// Construct an initializer for a BinArray that's nbytes long
	InitBinArray(std::size_t nbytes) : nbytes_(nbytes) {}

	/// InitBinArray is callable like a function
	void operator()(BinArray* target) {
		// Use placement-new to initialize the target with the BinArray constructor.
		new(target) BinArray(nbytes_);
	}

private:
	std::size_t nbytes_;
}
```

InitBinArray will install the new BinArray's header, and nothing else.

Implement the `BinArray` allocator in `include/Splash/Allocators.hpp`:

```c++
inline BinArray* allocateBinArray(OMR::GC::Context& cx, std::size_t nbytes) noexcept {
	return OMR::GC::allocateNonZero<BinArray>(cx, binArraySize(nbytes), InitBinArray(nbytes));
}
```

The allocator does not need to zero the new array's data, because a BinArray's slots are not scanned by the collector. We can use `OMR::GC::allocateNonZero` to allocate from non-zero-initialized memory, which makes allocating BinArrays faster.

### Implement the RefArray allocator

Implement the primitive-initializer for `RefArray` in `include/Splash/Allocators.hpp`:

```c++
struct InitRefArray {
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
inline RefArray* allocateRefArray(OMR::GC::Context& cx, std::size_t nrefs) noexcept {
	return OMR::GC::allocate<RefArray>(cx, refArraySize(nrefs), InitRefArray(nrefs));
}
```
This ensures that the references will be NULL, and the new array is safe to walk.

## Task 3: Implement and run the GC benchmark (15 minutes)

Our benchmark today is extremely simple. The benchmark will allocate a single RefArray with 1000 elements. This is our one root object. Then we'll store BinArrays of varying size into the RefArray at varying indexes, over and over. The RefArray is rooted. This is a toy benchmark, and not representative of how a real application behaves. Regardless, it can still be an interesting exercise.

Implement the GC benchmark in `main.cpp`:

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
# in splash2018-gc-arrays:
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
./main
```

The collector can output verbose logs, which you can inspect to see what GC operations are taking place. You should see a large number of global collections due to allocation failures or TLH refresh failures. You'll likely have quite   few issues to sort out :)

Run the benchmark with verbose logs enabled:

```bash
# in build/
export OMR_GC_OPTIONS"-Xverbosegclog:stderr"
./main
```

 Remember, OMR has been built without optimizations, so you can expect gc performance to be much worse than malloc. Compile the project in release mode, and run the benchmark again:

```bash
# in splash2018-gc-arrays/
mkdir build_release
cd build_release
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
export OMR_GC_OPTIONS=""
./main
```

How do the times compare?

### Optional: Implement a printing visitor

Optionally, you might want to implement a visitor that prints the references in a `RefArray`. The visitor could be used to validate your object scanner.

```c++
class PrintingVisitor {
public:
	explicit PrintingVisitor() {

	template <typename SlotHandleT>
	bool edge(void* object, SlotHandleT slot) {
		std::cerr << "#<slot" 
		          << " :object " << object
				  << " :address "<< slot.toAddress()
				  << " :value "  << slot.readReference()
				  << ">\n";
	}
};
```

If you're having crashes, you might use this visitor at the start of every iteration, to print the slots in the root `RefArray`.

## Task 3: Enable heap compaction (10 minutes)

As the mutator executes, objects will be allocated and freed dynamically. As objects are freed, these spaces become "holes" of free memory in the heap. Sometimes, these "holes" are too small to be reused, and live on as fragments of unusable memory. Heap fragmentation is especially problematic for long-running processes: Fragmentation results in:

1. Increased total memory consumption (holes are a waste of "in use" memory)
2. Slower applications, by hurting cache locality
4. Unexpected out-of-memory conditions, when free memory can't be reused.

To combat heap fragmentation, the collector can perform a sliding compaction of live objects, grouping objects together, leaving a contiguous span of free space at the end of the heap.

Your task is to enable the compactor, and measure it's affect on our benchmark:

1. Enable OMR_GC_MODRON_COMPACTOR in OmrConfig.cmake
2. Recompile the project
3. Run the benchmark
4. Turn on verbose GC logs and watch for compact phases

### Turn on the compaction build flag

In `OmrConfig.cmake`, enable the flag:

```cmake
set(OMR_GC_MODRON_COMPACTION ON  CACHE INTERNAL "")
```

### Watch the verbose GC logs

Rebuild the project in debug mode, and enable the compactor runtime option:

```bash
# in build/
export OMR_GC_OPTIONS="-Xcompactgc -Xverbosegclog:stderr"
```

You should be able to see the compactor sliding 1001 objects at each global collection.

### Run the benchmark with the compactor

Run the benchmark with the compactor enabled, in release mode:

```bash
# in build_release/
export OMR_GC_OPTIONS="-Xcompactgc"
```

How does the compactor affect our benchmark time?

## Task 5: Enable the scavenger (time 20 minutes)

In OMR, the scavenger is our semispace-copying generational collector. This task is to use the scavenger to improve GC time.

Turn on the scavenger compile flag in `OmrConfig.cmake`:

```cmake
set(OMR_GC_MODRON_SCAVENGER  ON CACHE INTERNAL "")
```

### Teach the scavenger to read the size of objects from the header

The scavenger preserves the first word of an object when moving--this is our ArrayHeader.
The word is embedded in the `ForwardedHeader`, a special on-heap structure that contains the new address of an object. In some cases, the scavenger will need to determine the size of an object from it's preserved header slot.

Implement the `MM_ObjectModelDelegate` member-function `getForwardedObjectSizeInBytes` in `glue/include/ObjectModelDelegate.hpp`:

```c++
MMINLINE uintptr_t
getForwardedObjectSizeInBytes(MM_ForwardedHeader *forwardedHeader)
{
  // Construct a temporary array header on the stack by copying the preserved heap slot.
  // Use the on-stack header to determine the original object's size.
  Splash::ArrayHeader header((Splash::Kind)-1, 0);
  header.value = forwardedHeader->getPreservedSlot();
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
                  std::size_t index, Ref value) {
  OMR::GC::store(cx, (Ref)&array, at(array, index), value);
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

Thank you very much for participating in this guide!

## Bonus Task: Implement compressed references (not done yet)

In this task we're going to use a smaller width datatype to represent our references. This will give our reference arrays better density. 

There is an issue though, we cannot represent every possible 64bit address.

If we chose to represent references in a 32bit property, we know that the maximum heap address we can represent is 2^32, or 4gib. However, we can play with object alignment to increase that limit dramatically on 64 bit systems.

If we know objects sizes are always a multiple of 2, then every address on the heap must be even, and thus have a zero in the least significant bit. That's one bit that's zero in every valid reference! We can shift the heap addresses right by one bit as we compress, we've effectively doubled the maximum heap address we can represent in 32 bits.

The table below shows the relationship between object alignment and maximum address in a 32bit value.

Alignment (bytes) | N (free bits) | Max Heap Address (gib)
------------------|---------------|-----------------------
 1                | 0             |   4
 2                | 1             |   8
 4                | 2             |  16
 8                | 3             |  32
16                | 4             |  64
32                | 5             | 128


Today, our object alignment is 4 bytes, which gives us a compression shift of 3, and a maximum heap address of 16gib.

1. In `RefArray`, make the data an array of `std::uint32_t`
2. Add the constant COMPRESSED_REF_SHIFT = 4 in `Arrays.hpp`
3. Have the slot handle helper create a  `OMR::GC::CompressedRefSlotHandle`, with shift 4.
4. Make the object scanner create compressed ref slot-handles.
