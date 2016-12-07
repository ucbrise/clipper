# Define library. Only source files here!
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(libclipper VERSION 0.1.0 LANGUAGES CXX)

link_directories(../protos)

add_library(clipper
    src/query_processor.cpp
    src/datatypes.cpp
    src/selection_policies.cpp
    src/persistent_state.cpp
    src/task_executor.cpp
    src/rpc_service.cpp
    src/timers.cpp
    src/containers.cpp
    )


# Define headers for this library. PUBLIC headers are used for
# compiling the library, and will be added to consumers' build
# paths.
target_include_directories(clipper PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_include_directories(clipper PUBLIC ../flatbuffers)

include_directories(${ProtobufIncludePath})
include_directories(../flatbuffers) # NOT CORRECT FOR AUTOMATIC COMPILATION

target_link_libraries(clipper boost zmqcpp protos)

# This makes the project importable from the build directory
export(TARGETS clipper FILE ClipperConfig.cmake)

# TODO Look at googletest to see how to include tests
# Every library has unit tests, of course
# add_executable(testlib
#     test/testlib.cpp)
#
# target_link_libraries(testlib
#     lib)
#
# add_test(testlib testlib)
#
