find_package(Protobuf REQUIRED)
file(GLOB ProtoFiles "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")
PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders ${ProtoFiles})
add_library(protos STATIC ${ProtoSources} ${ProtoHeaders})
target_link_libraries(protos ${PROTOBUF_LIBRARY})
set(ProtobufIncludePath ${CMAKE_CURRENT_BINARY_DIR}
        CACHE INTERNAL "Path to generated protobuf files.")
include_directories(${ProtobufIncludePath})