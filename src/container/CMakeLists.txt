cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# C++ container static library for use with Rcpp

add_library(container_lib STATIC src/container_rpc.cpp)
target_link_libraries(container_lib zmqcpp)
target_include_directories(container_lib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)

##################################
# Container test suite

add_executable(container_tests EXCLUDE_FROM_ALL
        src/container_tests.cpp
        )
target_link_libraries(container_tests gtest gmock_main clipper container_lib)
add_dependencies(unittests container_tests)

##################################
# Container executable for rpc testing

add_executable(container_rpc_test EXCLUDE_FROM_ALL
        src/container_rpc_test.cpp)

target_link_libraries(container_rpc_test clipper zmqcpp container_lib cxxopts)

##################################
# R support library dev - DELETE BEFORE MERGING

add_library(r_container SHARED R/r_models.cpp)
target_link_libraries(r_container container_lib zmqcpp rcpp)
