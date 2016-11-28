cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
##########################################
### ZeroMQ
##########################################
# Find and link zeromq installed library
find_package(ZeroMQ 4.1.6 REQUIRED)
add_library(zeromq INTERFACE IMPORTED)
set_property(TARGET zeromq PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES ${ZeroMQ_INCLUDE_DIRS})
set_property(TARGET zeromq PROPERTY
        INTERFACE_LINK_LIBRARIES ${ZeroMQ_LIBRARIES})

# Include zeromq C++ wrapper
add_library(zmqcpp INTERFACE)
target_include_directories(zmqcpp INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zmq>
        $<INSTALL_INTERFACE:zmq>)
target_link_libraries(zmqcpp INTERFACE zeromq)
export(TARGETS zmqcpp FILE ZeroMQConfig.cmake)

##########################################
### HTTPServer
##########################################
# Include httpserver library
add_library(httpserver INTERFACE)
target_include_directories(httpserver INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/httpserver>
    $<INSTALL_INTERFACE:httpserver>)
export(TARGETS httpserver FILE HttpServerConfig.cmake)

# ##########################################
# ### GoogleTest
# ##########################################

# include(ExternalProject)
#
# ExternalProject_Add(googletest
#     GIT_REPOSITORY "https://github.com/google/googletest"
#     GIT_TAG "release-1.8.0"
#     DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}
#     PREFIX ${CMAKE_CURRENT_BINARY_DIR}
#     # DOWNLOAD_DIR ${CMAKE_CURRENT_BINARY_DIR}/googletest
#     # PREFIX ${CMAKE_CURRENT_BINARY_DIR}/googletest
#     INSTALL_COMMAND ""
# )
#
# ExternalProject_Get_Property(googletest SOURCE_DIR)
# set(GTEST_INCLUDE_DIRS ${SOURCE_DIR}/googletest/include PARENT_SCOPE)
#
# ExternalProject_Get_Property(googletest BINARY_DIR)
# set(GTEST_LIBS_DIR ${BINARY_DIR}/googlemock/gtest PARENT_SCOPE)
#

