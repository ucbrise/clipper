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

add_executable(rpctest src/rpc_service_test.cpp)

target_link_libraries(rpctest clipper)

##################################