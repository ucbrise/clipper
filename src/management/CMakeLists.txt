cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# Management REST frontend executable

add_executable(management_frontend src/management_frontend_main.cpp)

target_link_libraries(management_frontend clipper redox boost httpserver cxxopts)

##################################
# Tests for management

add_executable(managementtests EXCLUDE_FROM_ALL
    src/management_tests.cpp
    src/management_frontend_tests.cpp
    )
target_link_libraries(
    managementtests
    clipper
    httpserver
    redox
    gtest
    gmock_main cxxopts)
add_dependencies(unittests managementtests)
