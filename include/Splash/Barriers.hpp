#if !defined(SPLASH_BARRIERS_HPP_)
#define SPLASH_BARRIERS_HPP_

#include <Splash/Arrays.hpp>
#include <OMR/GC/System.hpp>
#include <OMR/GC/AccessBarrier.hpp>
#include <OMR/GC/RefSlotHandle.hpp>

namespace Splash {

/// Return a slot to the handle at I
inline OMR::GC::RefSlotHandle at(RefArray& array, std::size_t index) {
  return OMR::GC::RefSlotHandle(&array.data[index]);
}

/// Store a ref to the slot at index.
inline void store(OMR::GC::RunContext& cx, RefArray& array, std::size_t index, Ref value) {
  OMR::GC::store(cx, (Ref)&array, at(array, index), (Ref)value);
}

inline void store(OMR::GC::RunContext& cx, OMR::GC::Handle<RefArray> array, std::size_t index, Ref value) {
  OMR::GC::store(cx, (AnyArray*)array.get(), at(*array, index), (AnyArray*)value);
}

// inline Ref load(OMR::GC::Context& cx, RefArray& array, std::size_t index) {
// 	return OMR::GC::load(cx, &array, at(array, index));
// }

} // namespace Splash

#endif // endif // SPLASH_BARRIERS_HPP_
