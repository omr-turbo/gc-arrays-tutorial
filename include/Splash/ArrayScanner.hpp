#if !defined(SPLASH_ARRAYSCANNER_HPP_)
#define SPLASH_ARRAYSCANNER_HPP_

#include <Splash/Arrays.hpp>
#include <OMR/GC/ScanResult.hpp>
#include <stdexcept>
#include <cassert>

namespace Splash {

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

}  // namespace Splash

#endif // SPLASH_ARRAYSCANNER_HPP_
