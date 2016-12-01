cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

find_package(flatbuffers REQUIRED)

FLATBUFFERS_GENERATE_C_HEADERS(RPC_FLATBUFFERS rpc.fbs)
add_custom_target(generate_flatbuffer_headers DEPENDS ${RPC_FLATBUFFERS_OUTPUTS})
include_directories(${RPC_FLATBUFFERS_OUTPUTS})