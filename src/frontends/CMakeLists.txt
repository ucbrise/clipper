cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

##################################
# Query REST frontend executable

add_executable(query_frontend src/query_frontend_main.cpp)

target_link_libraries(query_frontend clipper boost httpserver cxxopts rapidjson)

##################################
# Tests for frontends

add_executable(frontendtests EXCLUDE_FROM_ALL
    src/frontend_tests.cpp
    src/query_frontend_tests.cpp
    )
target_link_libraries(frontendtests clipper httpserver gtest gmock_main rapidjson cxxopts)
add_dependencies(unittests frontendtests)
