#if !defined(SPLASH_ALLOCATORS_HPP_)
#define SPLASH_ALLOCATORS_HPP_

#include <Splash/Arrays.hpp>
#include <OMR/GC/Allocator.hpp>

namespace Splash {

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
};

inline BinArray* allocateBinArray(OMR::GC::Context& cx, std::size_t nbytes) noexcept {
	return OMR::GC::allocateNonZero<BinArray>(cx, binArraySize(nbytes), InitBinArray(nbytes));
}

struct InitRefArray {
public:
	InitRefArray(std::size_t nrefs) : nrefs_(nrefs) {}

	void operator()(RefArray* target) {
		new (target) RefArray(nrefs_);
	}

	std::size_t nrefs_;
};

inline RefArray* allocateRefArray(OMR::GC::Context& cx, std::size_t nrefs) noexcept {
	return OMR::GC::allocate<RefArray>(cx, refArraySize(nrefs), InitRefArray(nrefs));
}

} // namespace Splash

#endif // SPLASH_ALLOCATORS_HPP_
