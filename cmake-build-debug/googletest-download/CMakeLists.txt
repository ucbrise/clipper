cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

project(googletest-download NONE)

include(ExternalProject)
ExternalProject_Add(googletest-download
                    GIT_REPOSITORY;https://github.com/google/googletest.git;GIT_TAG;master;UPDATE_DISCONNECTED;1
                    SOURCE_DIR          "/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/googletest-src"
                    BINARY_DIR          "/Users/tim/Documents/Clipper_proj/clipper/cmake-build-debug/googletest-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
)
