# Define library. Only source files here!
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

# Include httpserver library
add_library(httpserver INTERFACE)
target_include_directories(httpserver INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/httpserver>
    $<INSTALL_INTERFACE:httpserver>)
export(TARGETS httpserver FILE HttpServerConfig.cmake)
