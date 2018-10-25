###############################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/omr/cmake/modules/
)

project(splash2018-gc-arrays
    LANGUAGES CXX C
)

include(OmrPlatform)
include(OmrConfig.cmake)

omr_platform_global_setup()

### Subdirectories

add_subdirectory(glue)
add_subdirectory(omr)

### Splash Base

add_library(splash_base INTERFACE)

target_compile_definitions(splash_base
    INTERFACE
        # Required by OMR
        -DUT_DIRECT_TRACE_REGISTRATION
)

target_include_directories(splash_base
    INTERFACE
        include/
        glue/include/
        # Required by OMR
        ${omr_SOURCE_DIR}/include_core
        ${omr_BINARY_DIR}
        ${omr_BINARY_DIR}/include_core
)

### Main executable

add_executable(main
    main.cpp
)

target_link_libraries(main
    PUBLIC
        splash_base
        omrgc
)

target_compile_features(main
    PUBLIC
        cxx_std_11
)

