cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# Benchmark executable

add_executable(end_to_end_bench src/end_to_end_bench.cpp src/bench_utils.cpp src/bench_utils.hpp)

target_link_libraries(end_to_end_bench clipper boost cxxopts)


##################################
# RPC Layer Benchmarking executable

add_executable(rpcbench src/rpc_service_bench.cpp)

target_link_libraries(rpcbench clipper cxxopts)


##################################
# RPC Layer Protocol Testing executable

add_executable(rpctest src/rpc_protocol_test.cpp)

target_link_libraries(rpctest clipper cxxopts)
