/*******************************************************************************
 *  Copyright (c) 2018, 2018 IBM and others
 *
 *  This program and the accompanying materials are made available under
 *  the terms of the Eclipse Public License 2.0 which accompanies this
 *  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 *  or the Apache License, Version 2.0 which accompanies this distribution and
 *  is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 *  This Source Code may also be made available under the following
 *  Secondary Licenses when the conditions for such availability set
 *  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 *  General Public License, version 2 with the GNU Classpath
 *  Exception [1] and GNU General Public License, version 2 with the
 *  OpenJDK Assembly Exception [2].
 *
 *  [1] https://www.gnu.org/software/classpath/license.html
 *  [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 *  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(OMRCLIENT_GC_OBJECTSCANNER_HPP_)
#define OMRCLIENT_GC_OBJECTSCANNER_HPP_

#include <OMRClient/GC/ObjectRef.hpp>
#include <Splash/ArrayScanner.hpp>

namespace OMRClient {
namespace GC {

/// A "do nothing" default scanner.
class NopScanner {
public:
	NopScanner() = default;

	NopScanner(const NopScanner&) = default;

	template <typename VisitorT>
	OMR::GC::ScanResult
	start(VisitorT&& visitor, ObjectRef target, std::size_t bytesToScan = SIZE_MAX) noexcept {
		return {0, true};
	}

	template <typename VisitorT>
	OMR::GC::ScanResult
	resume(VisitorT&& visitor, std::size_t bytesToScan = SIZE_MAX) noexcept {
		return {0, true};
	}
};

/// Simple alias used by OMR to scan objects.  Internally in OMR, the symbol OMRClient::GC::ObjectScanner is used.
/// This header is provided by the client to "bind" that name to the correct implementation.
/// In our case, it's the ArrayScanner.

// SPLASH TODO: Implement the ArrayScanner, and set the alias below.

using ObjectScanner = NopScanner;

}  // namespace GC
}  // namespace OMR

#endif // OMRCLIENT_GC_OBJECTSCANNER_HPP_
