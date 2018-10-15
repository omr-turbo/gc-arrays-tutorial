/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "ObjectModel.hpp"

#include <cstdio>
#include <cassert>

omrobjectptr_t
GC_ObjectModelDelegate::initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization)
{
	assert(0);
	return nullptr;
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
GC_ObjectModelDelegate::calculateObjectDetailsForCopy(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *reservedObjectSizeInBytes, uintptr_t *hotFieldAlignmentDescriptor)
{

	*objectCopySizeInBytes = getForwardedObjectSizeInBytes(forwardedHeader);
	*reservedObjectSizeInBytes = env->getExtensions()->objectModel.adjustSizeInBytes(*objectCopySizeInBytes);
	*hotFieldAlignmentDescriptor = 0;

	//fprintf(stderr, "(copy-details :new-size %lu :old-size %lu)\n", *reservedObjectSizeInBytes, *objectCopySizeInBytes);
}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
