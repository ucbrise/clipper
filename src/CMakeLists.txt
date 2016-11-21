# Resources:
#   - https://rix0r.nl/blog/2015/08/13/cmake-guide/
#   - http://www.slideshare.net/DanielPfeifer1/cmake-48475415


cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project("Clipper" VERSION 0.1.0 LANGUAGES CXX)
include(cmake/FindZeroMQ.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include Boost as an imported target
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.62.0 REQUIRED COMPONENTS thread system chrono date_time atomic)
# message(STATUS "Boost_INCLUDE_DIR: " ${Boost_INCLUDE_DIRS})
# message(STATUS "Boost_LINK_DIR: " ${Boost_LIBRARY_DIRS})
# INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
# TARGET_LINK_LIBRARIES(mangaMeCLI ${Boost_LIBRARIES})
# find_package(Boost 1.62.0 REQUIRED)
add_library(boost INTERFACE IMPORTED)
set_property(TARGET boost PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIRS})
set_property(TARGET boost PROPERTY
    INTERFACE_LINK_LIBRARIES ${Boost_LIBRARIES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra -Werror -Wno-deprecated-declarations")

# add_subdirectory(libs/extlib EXCLUDE_FROM_ALL)

# set(CLIPPER_VERSION_MAJOR 0)
# set(CLIPPER_VERSION_MINOR 1)
# set(CLIPPER_VERSION_PATH 0)

# be nice to vim users
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Require C++11
# http://bit.ly/2eNjvLs (StackOverflow post)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# disallow non-standard extensions
set(CMAKE_CXX_EXTENSIONS off)

# add shared C++ definitions
add_definitions( -DBOOST_THREAD_VERSION=4)

# Targets that we develop
enable_testing()
add_subdirectory(libclipper)
add_subdirectory(frontends)
add_subdirectory(libs)
