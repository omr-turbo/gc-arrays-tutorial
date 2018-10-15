cmake_minimum_required(VERSION 3.12)

project(making-an-object-model
    LANGUAGES CXX C
    DESCRIPTION "A tutorial on building an object model for the OMR GC"
    HOMEPAGE_URL "github.com/rwy0717/making-an-object-model"
)

list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/third_party/omr/cmake/modules/
)

include(OmrPlatform)

# Global Configuration

omr_platform_global_setup()

include(./OmrConfig.cmake)

add_definitions(-DUT_DIRECT_TRACE_REGISTRATION)

set(OMR_TRACE_ROOT ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "")

add_subdirectory(glue)

add_subdirectory(third_party/omr)

add_library(splash_base INTERFACE)

target_include_directories(splash_base
    INTERFACE
        include/
        glue/include/
        ${omr_SOURCE_DIR}/include_core
        ${omr_BINARY_DIR}
        ${omr_BINARY_DIR}/include_core
)

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
