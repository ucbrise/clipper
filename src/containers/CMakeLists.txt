cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# C++ container executable for testing

add_executable(cpp_container src/container_main.cpp)

target_link_libraries(cpp_container clipper zmqcpp src/container_rpc.cpp src/container_parsing.cpp)


##################################
# C++ container static library for use with Rcpp

add_library(container_lib STATIC src/container_rpc.cpp src/container_parsing.cpp)
target_link_libraries(container_lib clipper zmqcpp)





