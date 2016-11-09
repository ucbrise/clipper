cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

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