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
	return (size + alignment - 1) & ~(alignment - 1);
}

// SPLASH TODO

} // namespace Splash

#endif // SPLASH_ARRAYS_HPP_
