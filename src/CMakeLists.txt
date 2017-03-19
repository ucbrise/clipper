cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project("Clipper")

# Targets that we develop
add_subdirectory(libclipper)
add_subdirectory(frontends)
add_subdirectory(management)
add_subdirectory(libs)
add_subdirectory(benchmarks)
