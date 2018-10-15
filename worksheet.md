# Splash 2018 Worksheet

In this workshop, you'll be defining simple array-like data structures, and using the OMR GC to automatically manage their memory for you.

## Task -1: Prerequisites and setup (-2 hours)

Ensure that you've cloned the skeleton project recursively:

```sh
git clone --recursive https://github.com/rwy0717/making-an-object-model
```

## Task 0: Implement the Array structures (30 minutes)

**Add all the code examples in this section to `include/Splash/Arrays.hpp`**

You will be using C++ structs to lay the objects out in memory.


### The `Splash::AnyArray`, `Splash::Ref`, and `omrobjectptr_t` Types

Forward-declare the `AnyArray` union:

```c++
union AnyArray;
```

The type `AnyArray*` is a "pointer-to an array with an unknown type". `AnyArray*` is used to signify that a pointer could refer to _either_ a `RefArray` or a `BinArray`. `AnyArray` is only ever used this way, and can never be actually instantiated. You will be defining this type further down.

Define `Ref`, an alias for `AnyArray*`:

```c++
using Ref = AnyArray*;
```

The `Ref` type is used to indicate a value is a "GC reference". We will be using `Ref` as our fundamental object pointer type. A ref can point to any type of array.

### Implement the object description glue

`omrobjectdescription.hpp` defines a set of pointer-type typedef. These are the fundamental types that OMR will use internally, when referring to object pointers.

In `glue/omrobjectdescription.hpp`, add the following code:

```c++
namespace Splash {
  union AnyArray;
}

typedef Splash::AnyArray* languageobjectptr_t;
typedef Splash::AnyArray* omrobjectptr_t;
typedef Splash::AnyArray* omrarrayptr_t;

typedef uintptr_t fomrobject_t;
typedef uintptr_t fomrarray_t;
typedef fomrobject_t Slot;
```

Some applications will make more exotic definitions, but for us, everything is "just" a `Splash::Ref`.

### The `Splash::ArrayHeader`

The `ArrayHeader` is a fixed-length field located at the beginning of every heap-allocated object. The header contains enough information to inspect an array:
1. The array kind
2. The array's length (in elements)
3. The GC metadata

The GC needs every on-heap object to have a valid and fully initialized header. Without this, the collector will not be able to interpret the contents of an object. The header is implemented as 64bit (8 byte) integer, which encodes four values. We can use shifting and masks to manually encode and decode the metadata. The table below describes the encoding:

| bytes | Property          |
|------:|-------------------|
|     0 | GC metatdata      |
| 1 - 2 | Unused            |
|     3 | Kind              |
| 4 - 7 | Size              |

Define the `Kind` enum, which represents the type of an array:

```c++
enum class Kind {
	REF, BIN
};
```

Define the `ArrayHeader` struct:

```c++
/// Metadata about an Array. Must be the first field of any heap object.
///
/// Encoding:
///
/// Bytes           | Property
/// ----------------|------------------
/// 0               | Metadata (08 bits)
///   1 2           | Padding  (16 bits)
///       3         | Kind     (08 bits)
///         4 5 6 7 | Size     (32 bits)
///
struct ArrayHeader {
  constexpr ArrayHeader(Kind k, std::uint32_t s) noexcept
    : value((std::uint64_t(s) << 32) | (std::uint64_t(k) << 24))
  {}

  /// The number of elements in this Array. Elements may be bytes or references.
  /// NOT the total size in bytes.
  constexpr std::uint32_t length() const {
    return std::uint32_t((value >> 32) & 0xFFFFFFFF);
  }

  /// The kind of Array this is: either a RefArray or BinArray
  constexpr Kind kind() const {
    return Kind((value >> 24) & 0xFF);
  }

  std::uint64_t value;
};
```

### The `Splash::BinArray` struct

Define the `BinArray` struct:

```c++
struct BinArray {
  constexpr BinArray(std::size_t nbytes)
    : header(Kind::BIN, nbytes), data() {}
 
  ArrayHeader header;
  std::uint8_t data[0];
};
```

The `BinArray` has a header as it's first field. The constructor will initialize the header with the correct metadata.

When we create a BinArray, we will "overallocate" the struct, with additional memory past the end, for actual data storage. The zero-length `data` field can be used to index into this trailing storage.

Define the `binArraySize` helper:

```c++
constexpr std::size_t binArraySize(std::size_t nbytes) {
  return OMR::align(sizeof(BinArray) + nbytes, 16);
}
```

The actual size of the array is calculated by adding the fixed size of the header, to the variable size of the data. The array object model will align objects to 16 byte address, by forcing every object size to be a multiple of 16.

### The `Splash::RefArray` struct

Define the `RefArray` struct:

```c++
struct RefArray {
  constexpr RefArray(std::size_t nrefs)
    : header(Kind::REF, nrefs), data() {}

  std::size_t length() const { return header.length(); }

  /// Returns a pointer to the first slot.
  Ref* begin() { return &data[0]; } 

  /// Returns a pointer "one-past-the-end" of the slots.
  Ref* end() { return &data[length()]; }

  ArrayHeader header;
  Ref data[0];
};
```

The `RefArray` is layed out very similarly to the `BinArray`. Again, the first element must be the `ArrayHeader`, and storage for the references will be allocated past the end of the struct. The RefArray has some additional member functions which will come in handy when we need to scan the arrray.

Define the `refArraySize` helper function:

```c++
constexpr std::size_t refArraySize(std::size_t nrefs) {
  return align(sizeof(RefArray) + (sizeof(Ref) * nrefs), 16);
}
```

### Define the AnyArray union

You have already forward-declared the `AnyArray` union, when defining the `Ref` type.
Now that the `RefArray` and `BinArray` types have been defined, you can write the actual definition.

Define the `AnyArray` union:

```c++
struct AnyArray {
  /// AnyArray is not constructible.
  AnyArray() = delete;

  union {
    ArrayHeader asHeader;
    RefArray asRefArray;
    BinArray asBinArray;
  };
};
```

To prevent users from instantiating the `AnyArray`, the constructor is explicitly deleted. Remember, `AnyArray` may only ever be used as a pointer-type.

Both the `RefArray` and `BinArray` have valid headers as their first field, which allows us to access the header directly, regardless of what type the array actually is.

Define the helper function `kind`, which returns the type of an `AnyArray` by reading the header:

```c++
/// Find the kind of array by reading from it's header.
constexpr Kind kind(AnyArray* any) {
  return any->asHeader.kind();
}
```

Further, it's possible to calculate an Array's size in bytes:

Define the helper function `size`, which uses the header to find the total size in bytes of any array:

```c++
/// Get the total allocation size of an array, in bytes.
inline std::size_t size(AnyArray* any) noexcept {
  switch(kind(any)) {
    case Kind::REF: return refArraySize(any->asHeader.length());
    case Kind::BIN: return binArraySize(any->asHeader.length());
    default: assert(0);
  }
}
```

## Task 1: Implement the `ArrayScanner` (1 hour)

**Add the code in this section to `include/Splash/ArrayScanner.hpp`**

The RefArray and BinArray types are already implemented. Your task is to implement and test an ArrayScanner class. Implementing the scanner can be surprisingly challenging. Luckily, we only have to do it once.

The object scanner is the primary interface between the collector and your objects. The scanner notifies the GC about where references are in objects, and how they are encoded.

The scanner has a small public API:

```c++
class ArrayScanner {
public:
	// In our case, the scanner is default constructible.
	ArrayScanner() = default;

	// The scanner _must be copyable
	ArrayScanner(const ArrayScanner&) = default;

	// begin scanning an object
	template <typename VisitorT>
	OMR::GC::ScanResult
	start(VisitorT&& visitor, AnyArray* target, std::size_t todo = SIZE_MAX) noexcept;

	// resume scanning an object
	template <typename VisitorT>
	OMR::GC::ScanResult
	resume(VisitorT&& visitor, std::size_t todo = SIZE_MAX) noexcept;
};
```

The scanner's job is to give visitors a handle to each reference-slot in the target object. The handle gives visitors a way to read or write from object-slots. Many GC operations will need to read or write to objects in some way. The OMR GC implements a variety of visitors to perform tasks on objects.

As an example, during a global marking phase, the `MarkingVisitor` will read object's reference slots to find which objects are reachable.

The scanner passes slot handles to the visitor's edge method, summarized below:

``` c++
template <typename SlotHandle>
bool VisitorT::edge(void* object, SlotHandle&& handle) noexcept;
```

The visitor's `edge` member-function returns a bool. If the result is true, scanning can continue normally. However, if the visitor returns `false`, The scanner should pause. This might happen if the collector is perfoming an incremental or concurrent task that needs to be interrupted early. If the scan is paused, the collector may resume scanning the current target by calling `ArrayScanner::resume`. It is the scanner's responsibility to track the target and current scan state.

The most basic slot-handle is the `OMR::GC::RefSlotHandle`, that is, a _handle_ to a _slot_ containing a plain (unencoded) reference. Think of it like a `void**`.  This is the slot handle type you'll be using today. It's possible to use a custom slot-handle type to implement your own reference encoding.

The scanner's result is an OMR::GC::ScanResult, duplicated below:

```c++
struct ScanResult
{
	std::size_t bytesScanned;
	bool complete;
};
```

It is the scanner's responsibility to track how many bytes were scanned, and report that amount in the result.
If the scan was interrupted before the object was completed, `result.complete` must be false.

The scanner's start and resume API both take a scan budget. If the number of bytes scanned reaches (or exceeds) this budget, the scanner should save it's state and return early. The ScanResult should report `complete` is `false`.

Bonus: If the visitor returns `false` on the last slot, the scanner should return `complete = true`.

Implement the `ArrayScanner` in `include/ArrayScanner.hpp`.

Summary:

Exit conditions:
- iterator has reached end of object
- bytes scanned i
- visitor has returned false

Return value:
- bytes actually scanned
- whether object has completed scanning.

```
for (Ref& )
  if (is-reference(slot))
    continue = visitor.edge(target, to-handle(slot))
	if (!continue)
	  return { complete: false, bytesScanned: 0}
```

#### 

### Optional: Implement a printing visitor

Implement a printing visitor to test your scanner.

```c++
class PrintingVisitor {
public:
	explicit PrintingVisitor() 

	explicit ExpectVisitor(std::vector<AnyArray*>&& references)
		: references(std::move(references)), index(0) {}

	template <typename SlotHandleT>
	bool edge(void* object, SlotHandleT slot) {
		std::cerr << "#(slot" 
		          << " :object " << object
				  << " :address "<< slot.toAddress()
				  << " :value "  << slot.readReference()
				  << ")\n";
	}

private:
	std::vector references;
	std::size_t index;
};
```

```c++

int main() {
	auto array0 = (Splash::RefArray*) malloc (Splash::refArraySuze)
}
```

### 

## Task 2: Implement the Array allocators (time 30 minutes)

**Add the code in this section to `include/Splash/Allocator.hpp`**


The OMR GC allocation APIs you will be using are duplicated below:

```c++
template <typename T = void, typename Init>
T* allocate(Context& cx, std::size_t size, Init&& init) noexcept;

template <typename T = void, typename Init>
T* allocateNonZero(Context& cx, std::size_t size, Init&& init) noexcept;
```

Each allocator takes a "primitive initializer" function (or function-like object). The initializer is able to clear a newly-allocated object, and put it into a consistent state for the collector to scan. The initializer function may not allocate and may not fail. From an "application" perspective, the object may not be fully initialized.

Every allocation could potentially do some garbage collection work. This is why even objects currently being allocated must be in a scannable state--the GC may attempt to scan it before allocate returns.

### Implement the BinArray allocator

Implement the "primitive initializer" for `BinArray`:

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

The initializer is creating the header, and nothing else.

Implement the `BinArray` allocator:

```c++
inline BinArray* allocateBinArray(OMR::GC::Context& cx, std::size_t nbytes) noexcept {
	return OMR::GC::allocateNonZero<BinArray>(cx, binArraySize(nbytes), InitBinArray(nbytes));
}
```

Because the BinArray's slots are not scanned by the collector, the primitive initializer does not need to zero the target's data. We use `OMR::GC::allocateNonZero` to make allocating bin arrays a little faster.

### Implement the RefArray allocator

Implement the primitive-initializer for `RefArray`:

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

Again, the initalizer is just creating the header. However, the new array will need it's reference-slots cleared, in order to be safetly walkable. Rather than zero the slots in the initializer, we can allocate the object from pre-zeroed memory.

Implement the `RefArray` allocator:

```c++
inline RefArray* allocateRefArray(OMR::GC::Context& cx, std::size_t nrefs) noexcept {
	return OMR::GC::allocate<RefArray>(cx, refArraySize(nrefs), InitRefArray(nrefs));
}
```

By using the plain `allocate` call, OMR will make the allocation out of zero-initialized memory. This ensures that the references will be NULL, and the new array is safe to walk.

## Task 4: Implement and run the GC benchmark

Implement the GC benchmark in main.cpp:

```c++

__attribute__((noinline))
void gc_bench(OMR::GC::RunContext& cx) {
  OMR::GC::StackRoot<Splash::RefArray> root(cx);
  root = Splash::allocateRefArray(cx, ROOT_SIZE);
  for (std::size_t i = 0; i < ITERATIONS; ++i) {
    auto child = (Splash::AnyArray*)Splash::allocateBinArray(cx, childSize(i));
    root->data[index(i)] = child;
  }
}
```

Compile the project in release mode:

```bash
cd $project

mkdir build_release
cd build_release
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
```


Run the benchmark:

```bash
# in build_release
./main
```

How do the times compare?

## Task 3: Enable the scavenger

In OMR, the scavenger is our basic generational collector.

Subtasks:
- Update the Slot accessors to use OMR write barriers to store into objects
- In OmrConfig.cmake, enable the flag `OMR_GC_MODRON_SCAVENGER`
- Rebuild the project
- Rerun the test
- Enable the verbose GC logs, and watch for scavenge events. They should greatly outnumber global collections.


### Modify the benchmark

```c++
__attribute__((noinline))
void gc_bench(OMR::GC::RunContext& cx) {
  OMR::GC::StackRoot<Splash::RefArray> root(cx);
  root = Splash::allocateRefArray(cx, ROOT_SIZE);
  for (std::size_t i = 0; i < ITERATIONS; ++i) {
    auto child = (Splash::AnyArray*)Splash::allocateBinArray(cx, childSize(i));
    Splash::store(cx, *root, index(i), child);
  }
}
```

### Turn on the scavenger

### Watch the verbose GC logs to see when the collector scavengers

### Test the throughput

## Task 4: Enable heap compaction

As the mutator executes, objects will be allocated and freed dynamically. As objects are freed, these spaces become "holes" of free memory in the heap. Sometimes, these "holes" are too small to be reused, and live on as fragments of unusable memory. Heap fragmentation is bad, especially for long-running processes: Fragmentation results in:

1. Increased total memory consumption (holes are a waste of "in use" memory)
2. Slower applications, by hurting cache locality
4. Unexpected out-of-memory conditions, when free memory can't be reused.

To combat heap fragmentation, the collector can perform a sliding compaction of live objects, grouping free space at the end of the heap. This effectively eliminates heap fragmentation, while pushing live objects closer together.

Your task is to enable the compactor, and measure it's affect on our benchmark:

1. Enable OMR_GC_MODRON_COMPACTOR in OmrConfig.cmake
2. Recompile the project
3. Run the benchmark
4. Turn on verbose GC logs and watch for compact phases

### Turn on the compile flags

### Watch the verbose GC logs

### Test the thoughput of our test

## Task 5: Implement compressed references

In this task we're going to use a smaller width datatype to represent our references. This will give our reference arrays better density. 

There is an issue though, we cannot represent every possible 64bit address.

If we chose to represent references in a 32bit property, we know that the maximum heap address we can represent is 2^32, or 4gib. However, we can play with object alignment to increase that limit dramatically on 64 bit systems.

If we know objects sizes are always a multiple of 2, then every address on the heap must be even, and thus have a zero in the 
least significant bit. That's one bit that's zero in every valid reference! We can shift the heap addresses right by one bit as we compress, we've effectively doubled the maximum heap addresss we can represent in 32 bits.

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

1. Use a `std::uint32_t` as the slot type in RefArrays.
1. Add the constant COMPRESSED_REF_SHIFT = 4.
2. Have the slot handle helper create a  `OMR::GC::CompressedRefSlotHandle`.
3. Use the compressed re
4. Define the type CompressedSlot as a uint32_t.
5.
1. Switch AnyArrayRef with Compres


## Task 6: Test application performance when optimized

Reconfigure the build to be release:

```sh
mkdir build_release
cd build_release
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja
```

And build the test application

```sh
ninja
```

Then run and time the two benchmarks:

```sh
./main
```

How has the duration changed from the debug build? Does OMR do better or worse?

# Solution
