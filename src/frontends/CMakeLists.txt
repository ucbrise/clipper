cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# Benchmark executable

add_executable(bench src/bench.cpp)

target_link_libraries(bench clipper boost)

##################################
# Query REST frontend executable

add_executable(query_frontend src/query_frontend_main.cpp)

target_link_libraries(query_frontend clipper boost httpserver cxxopts)

##################################
# RPC Layer Testing executable

add_executable(rpcbench src/rpc_service_bench.cpp)

target_link_libraries(rpcbench clipper)

##################################
# Tests for frontends

add_executable(frontendtests EXCLUDE_FROM_ALL
    src/frontend_tests.cpp
    src/query_frontend_tests.cpp
    )
target_link_libraries(frontendtests clipper httpserver gtest gmock_main)
add_dependencies(unittests frontendtests)
