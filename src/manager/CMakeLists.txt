cmake_minimum_required(VERSION 3.2 FATAL_ERROR)


##################################
# Manager executable

add_executable(manager src/manager.cpp)

target_link_libraries(manager clipper boost httpserver redox)

##################################
# REST HTTP Server executable

# add_executable(rest src/rest.cpp README.md)
#
# target_link_libraries(rest clipper boost httpserver)
#
