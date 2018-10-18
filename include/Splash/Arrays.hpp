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

#if !defined(SPLASH_ARRAYS_HPP_)
#define SPLASH_ARRAYS_HPP_

#include <objectdescription.h>

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
align(std::size_t size, std::size_t alignment) {
	return (size + alignment - 1) & ~(alignment - 1);
}

/////////////////////////////////////////////////
// SPLASH TODO:
// PUT YOUR CODE HERE!
/////////////////////////////////////////////////

} // namespace Splash

#endif // SPLASH_ARRAYS_HPP_
