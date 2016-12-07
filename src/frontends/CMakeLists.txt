cmake_minimum_required(VERSION 3.2 FATAL_ERROR)


##################################
# Benchmark executable

add_executable(bench src/bench.cpp)

target_link_libraries(bench clipper boost)

##################################
# REST HTTP Server executable

add_executable(rest src/rest.cpp README.md)

target_link_libraries(rest clipper boost httpserver)

##################################
# RPC Layer Testing executable

add_executable(rpctest src/rpc_service_bench.cpp)

target_link_libraries(rpctest clipper)

##################################
# Management REST HTTP Server executable

add_executable(admin_rest src/admin_rest.cpp)

target_link_libraries(admin_rest clipper boost httpserver)

##################################
# Tests for frontends

set(GTEST_COMMAND "${CMAKE_CURRENT_BINARY_DIR}/frontendstests")
add_executable(frontendstests EXCLUDE_FROM_ALL
    test/test_main.cpp
    test/rest_api_test.cpp
    )
target_link_libraries(frontendstests clipper gtest gmock_main httpserver)
add_dependencies(unittests frontendstests)
