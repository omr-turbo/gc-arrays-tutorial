###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

set(OMR_GC_GLUE_TARGET "splash_gc_glue" CACHE STRING "The gc glue target, must be interface library" FORCE)
set(OMR_UTIL_GLUE_TARGET "splash_util_glue" CACHE STRING "The util glue target, must be interface library" FORCE)
set(OMR_RAS_GLUE_TARGET "splash_ras_glue" CACHE STRING "The ras glue target, must be interface library" FORCE)
set(OMR_CORE_GLUE_TARGET "splash_core_glue" CACHE STRING "The core glue target, must be and interface library" FORCE)
set(OMR_STARTUP_GLUE_TARGET "splash_startup_glue" CACHE STRING "The startup glue target, must be an interface library" FORCE)

add_library(splash_gc_glue INTERFACE)

target_sources(splash_gc_glue INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/src/CollectorLanguageInterfaceImpl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ConcurrentMarkingDelegate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/FrequentObjectsStats.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/GlobalCollectorDelegate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/ObjectModelDelegate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/StartupManagerImpl.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/VerboseManagerImpl.cpp
)

target_link_libraries(splash_gc_glue
	INTERFACE
		splash_base
)

add_library(splash_util_glue INTERFACE)

target_sources(splash_util_glue INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/src/UtilGlue.c
)

target_link_libraries(splash_util_glue
	INTERFACE
		splash_base
)

add_library(splash_ras_glue INTERFACE)

target_sources(splash_ras_glue INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/src/Profiling.c
)

target_link_libraries(splash_ras_glue
	INTERFACE
		splash_base
)

add_library(splash_core_glue INTERFACE)

target_sources(splash_core_glue 
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/src/LanguageVMGlue.c
)

target_link_libraries(splash_core_glue
	INTERFACE
		splash_base
)

add_library(splash_startup_glue INTERFACE)
