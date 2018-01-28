# Install script for directory: /Users/tim/Documents/Clipper_proj/clipper/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/src/libclipper/cmake_install.cmake")
  include("/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/src/frontends/cmake_install.cmake")
  include("/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/src/management/cmake_install.cmake")
  include("/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/src/libs/cmake_install.cmake")
  include("/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/src/benchmarks/cmake_install.cmake")
  include("/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/src/container/cmake_install.cmake")

endif()

