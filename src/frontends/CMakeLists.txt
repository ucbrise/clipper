cmake_minimum_required(VERSION 3.2 FATAL_ERROR)


##################################
# Benchmark executable

add_executable(bench src/bench.cpp)

target_link_libraries(bench clipper boost)

##################################
# Query REST frontend executable

add_executable(query_frontend src/query_frontend.cpp)

target_link_libraries(query_frontend clipper boost httpserver)

##################################
# RPC Layer Testing executable

add_executable(rpcbench src/rpc_service_bench.cpp)

target_link_libraries(rpcbench clipper)

##################################
# Create library for frontends

# add_library(frontends
#     src/rest.cpp
#     )

# target_include_directories(frontends PUBLIC
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#     $<INSTALL_INTERFACE:include>
#     PRIVATE src)

# target_link_libraries(frontends clipper boost httpserver)

##################################
# Tests for frontends

# get_target_property(GTEST_COMMAND unittests COMMAND)
# set_property(TARGET unittests PROPERTY COMMAND "${GTEST_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/frontendtests")
add_executable(frontendtests EXCLUDE_FROM_ALL
    src/frontend_tests.cpp
    src/query_frontend_tests.cpp
    )
target_link_libraries(frontendtests clipper httpserver gtest gmock_main)
add_dependencies(unittests frontendtests)


