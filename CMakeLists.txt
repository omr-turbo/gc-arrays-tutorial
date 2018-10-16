cmake_minimum_required(VERSION 3.11)

project(splash2018-gc-arrays
    LANGUAGES CXX C
)

list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/omr/cmake/modules/
)

include(OmrPlatform)

# Global Configuration

omr_platform_global_setup()

include(./OmrConfig.cmake)

add_definitions(-DUT_DIRECT_TRACE_REGISTRATION)



add_subdirectory(glue)

add_subdirectory(omr)

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
