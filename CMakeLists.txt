# Resources:
#   - https://rix0r.nl/blog/2015/08/13/cmake-guide/
#   - http://www.slideshare.net/DanielPfeifer1/cmake-48475415

cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project("Clipper" VERSION 0.1.0 LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers-cmake")
include(FindZeroMQ)
find_package(Sanitizers)


# Include Boost as an imported target
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.58.0 REQUIRED COMPONENTS thread system chrono date_time atomic)
add_library(boost INTERFACE IMPORTED)
set_property(TARGET boost PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
set_property(TARGET boost PROPERTY
    INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})

# Include Folly as an imported target
find_package(Folly REQUIRED)
add_library(folly INTERFACE IMPORTED)
set_property(TARGET folly PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${FOLLY_INCLUDE_DIR})
set(FOLLY_LINK_FLAGS "-lglog -lcrypto -lssl")
set_property(TARGET folly PROPERTY
    INTERFACE_LINK_LIBRARIES ${FOLLY_LINK_FLAGS} ${FOLLY_LIBRARY})

# Include CityHash as an imported target
find_package(cityhash REQUIRED)
add_library(cityhash INTERFACE IMPORTED)
set_property(TARGET cityhash PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${CITYHASH_INCLUDE_DIR})
set_property(TARGET cityhash PROPERTY
        INTERFACE_LINK_LIBRARIES ${CITYHASH_LIBRARIES})

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -Werror -Wno-deprecated-declarations -Wno-unsupported-friend")
# Note(simon):
#   https://github.com/ucbrise/clipper/pull/595
#   pedantic is turned off for now until we figure out a way to turn off dlib warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-deprecated-declarations")
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unsupported-friend")
endif()

# be nice to vim users
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Require C++11
# http://bit.ly/2eNjvLs (StackOverflow post)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# disallow non-standard extensions
set(CMAKE_CXX_EXTENSIONS off)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED on)

# add shared C++ definitions
add_definitions( -DBOOST_THREAD_VERSION=4)

# set up dummy unittests target
add_custom_target(unittests)


### Google Test
# Reference: https://crascit.com/2015/07/25/cmake-gtest/
# enable_testing()
include(DownloadProject)
download_project(PROJ                googletest
                 GIT_REPOSITORY      https://github.com/google/googletest.git
                 GIT_TAG             master
                 UPDATE_DISCONNECTED 1
                 QUIET
)

# Prevent GoogleTest from overriding our compiler/linker options
# when building with Visual Studio
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

subdirs(src)
