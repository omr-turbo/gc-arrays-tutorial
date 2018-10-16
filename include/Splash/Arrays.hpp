#if !defined(SPLASH_ARRAYS_HPP_)
#define SPLASH_ARRAYS_HPP_

#include <OMR/GC/RefSlotHandle.hpp>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace Splash {

/// Round size up to a multiple of alignment.
/// @returns aligned size

constexpr std::size_t
align(std::size_t size, std::size_t alignment)
{
	return (size + alignment - 1) & ~(alignment - 1);
}

union AnyArray;

using Ref = AnyArray*;

enum class Kind : std::uint8_t {
	REF, BIN
};

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

struct BinArray {
	constexpr BinArray(std::uint32_t nbytes)
		: header(Kind::BIN, nbytes), data() {}
 
	ArrayHeader header;
	std::uint8_t data[0];
};

constexpr std::size_t binArraySize(std::uint32_t nbytes) {
	return align(sizeof(BinArray) + nbytes, 16);
}

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

constexpr std::size_t refArraySize(std::uint32_t nrefs) {
	return align(sizeof(RefArray) + (sizeof(Ref) * nrefs), 16);
}

union AnyArray {
	// AnyArray can not be constructed
	AnyArray() = delete;

	ArrayHeader asHeader;
	RefArray asRefArray;
	BinArray asBinArray;
};

/// Find the kind of array by reading from it's header.
constexpr Kind kind(AnyArray* any) {
	return any->asHeader.kind();
}

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

} // namespace Splash

#endif // SPLASH_ARRAYS_HPP_
