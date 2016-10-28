
project("clipper")

cmake_minimum_required_version(version 2.8 FATAL_ERROR)

set(CLIPPER_VERSION_MAJOR 0)
set(CLIPPER_VERSION_MINOR 1)
set(CLIPPER_VERSION_PATH 0)

# be nice to vim users
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# 
set(CMAKE_CXX_FLAGS_COMMON
    "-std=c++11 \
    -stdlib=libc++ \
    -Wall \
    -Wextra \
    -Werror")

set(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} \
    ${CMAKE_CXX_FLAGS_COMMON}")
set(CMAKE_CXX_FLAGS_RELEASE
    "${CMAKE_CXX_FLAGS_RELEASE} \
    ${CMAKE_CXX_FLAGS_COMMON}
    -O3")

enable_testing()

add_subdirectory(lib)
add_subdirectory(src)
