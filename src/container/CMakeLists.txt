cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# C++ container executable for testing

add_executable(cpp_container src/container_main.cpp)

target_link_libraries(cpp_container clipper zmqcpp src/container_parsing.cpp)


##################################
# C++ container static library for use with Rcpp

add_library(container_lib STATIC src/container_parsing.cpp)
target_link_libraries(container_lib clipper zmqcpp)
target_include_directories(container_lib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)





