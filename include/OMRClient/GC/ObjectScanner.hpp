#if !defined(OMRCLIENT_GC_OBJECTSCANNER_HPP_)
#define OMRCLIENT_GC_OBJECTSCANNER_HPP_

#include <Splash/ArrayScanner.hpp>

namespace OMRClient {
namespace GC {

/// Simple alias used by OMR to scan objects.  Internally in OMR, the symbol OMRClient::GC::ObjectScanner is used.
/// This header is provided by the client to "bind" that name to the correct implementation.
/// In our case, it's the ArrayScanner.

using ObjectScanner = ::Splash::ArrayScanner;

}  // namespace GC
}  // namespace OMR

#endif // OMRCLIENT_GC_OBJECTSCANNER_HPP_
