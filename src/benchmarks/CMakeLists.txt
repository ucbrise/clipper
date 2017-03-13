cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# Benchmark executable

add_executable(end_to_end_bench src/end_to_end_bench.cpp)

target_link_libraries(end_to_end_bench clipper boost)


##################################
# RPC Layer Testing executable

add_executable(rpcbench src/rpc_service_bench.cpp)

target_link_libraries(rpcbench clipper)

