# Define library. Only source files here!
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(libclipper VERSION 0.1.0 LANGUAGES CXX)


add_library(clipper
    src/query_processor.cpp
    src/datatypes.cpp
    src/metrics.cpp
    src/selection_policies.cpp
    src/persistent_state.cpp
    src/task_executor.cpp
    src/rpc_service.cpp
    src/timers.cpp
    src/containers.cpp
    src/redis.cpp
    src/logging.cpp
    src/json_util.cpp
    src/exceptions.cpp)


# Define headers for this library. PUBLIC headers are used for
# compiling the library, and will be added to consumers' build
# paths.
target_include_directories(clipper PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(clipper boost folly zmqcpp redox pthread rapidjson spdlog base64 cqueue cityhash dlib)

add_executable(libclippertests EXCLUDE_FROM_ALL
    test/test_main.cpp
    test/threadpool_test.cpp
    test/container_test.cpp
    test/metrics_test.cpp
    test/rpc_service_test.cpp
    test/timers_test.cpp
    test/data_serialization_tests.cpp
    test/persistent_state_test.cpp
    test/redis_test.cpp
    test/selection_policies_test.cpp
    test/json_util_test.cpp
    test/logging_test.cpp
    test/task_executor_test.cpp
    test/base64_test.cpp
    # config_test resets the value of the config so it should always
    # be last in the list
    test/config_test.cpp)

target_link_libraries(libclippertests clipper gtest gmock_main cxxopts)
add_dependencies(unittests libclippertests)

