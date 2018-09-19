cmake_minimum_required(VERSION 3.12)

project(making-an-object-model
    LANGUAGES CXX
    DESCRIPTION "A tutorial on building an object model for the OMR GC"
    HOMEPAGE_URL "github.com/rwy0717/making-an-object-model"
)

# add_subdirectory(third_party/omr)

add_executable(main
    main.cpp
)

target_link_libraries(main
    PUBLIC
#        omrgc
)

target_compile_features(main
    PUBLIC
        cxx_std_11
)

