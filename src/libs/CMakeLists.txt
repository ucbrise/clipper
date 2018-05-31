cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
##########################################
### ZeroMQ
##########################################
# Find and link zeromq installed library
find_package(ZeroMQ 4.1 REQUIRED)
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

##########################################
### CXXOpts Option Parser
##########################################

add_library(cxxopts INTERFACE)
target_include_directories(cxxopts INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/cxxopts>
    $<INSTALL_INTERFACE:cxxopts>)
export(TARGETS cxxopts FILE CxxOptsConfig.cmake)

##########################################
### Spdlog
##########################################

add_library(spdlog INTERFACE)
target_include_directories(spdlog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/spdlog/include>
    $<INSTALL_INTERFACE:spdlog>)
export(TARGETS spdlog FILE SpdlogConfig.cmake)

##########################################
### Base64
##########################################

add_library(base64 INTERFACE)
target_include_directories(base64 INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/base64>
        $<INSTALL_INTERFACE:base64>)
export(TARGETS base64 FILE Base64Config.cmake)

##########################################
### Concurrent Queue
##########################################

add_library(cqueue INTERFACE)
target_include_directories(cqueue INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/concurrent_queue>
        $<INSTALL_INTERFACE:cqueue>)
export(TARGETS cqueue FILE ConcurrentQueueConfig.cmake)

##########################################
### DLib Machine Learning
#########################################

add_subdirectory(dlib)

add_subdirectory(redox)

add_subdirectory(rapidjson)
