/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(OBJECTDESCRIPTION_H_)
#define OBJECTDESCRIPTION_H_

#include "omrcfg.h"

#include "omrcomp.h"
#include "omr.h"

namespace Splash {
  union AnyArray;
}

typedef Splash::AnyArray* languageobjectptr_t;  // object reference, used by langauge
typedef Splash::AnyArray* omrobjectptr_t;       // object reference, used by OMR internally
typedef Splash::AnyArray* omrarrayptr_t;        // array reference, used by OMR internally
typedef uintptr_t fomrobject_t;                 // object-reference field in object
typedef uintptr_t fomrarray_t;                  // array-reference field in object or array

#endif /* OBJECTDESCRIPTION_H_ */
