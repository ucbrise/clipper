cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# C++ container executable for testing

add_executable(cpp_container src/container_main.cpp)

target_link_libraries(cpp_container clipper zmqcpp)


##################################
# C++ container static library for use with Rcpp





