cmake_minimum_required(VERSION 2.4)

# Settings:
set(CTEST_DASHBOARD_ROOT                "/Users/Jay/clipper/deps/cmake-3.6.3/Tests/CTestTest")
set(CTEST_SITE                          "airbears2-10-142-37-154.airbears2.1918.berkeley.edu")
set(CTEST_BUILD_NAME                    "CTestTest-Darwin-g++-Crash")

set(CTEST_SOURCE_DIRECTORY              "/Users/Jay/clipper/deps/cmake-3.6.3/Tests/CTestTestCrash")
set(CTEST_BINARY_DIRECTORY              "/Users/Jay/clipper/deps/cmake-3.6.3/Tests/CTestTestCrash")
set(CTEST_CVS_COMMAND                   "CVSCOMMAND-NOTFOUND")
set(CTEST_CMAKE_GENERATOR               "Unix Makefiles")
set(CTEST_CMAKE_GENERATOR_PLATFORM      "")
set(CTEST_CMAKE_GENERATOR_TOOLSET       "")
set(CTEST_BUILD_CONFIGURATION           "$ENV{CMAKE_CONFIG_TYPE}")
set(CTEST_COVERAGE_COMMAND              "/usr/bin/gcov")
set(CTEST_NOTES_FILES                   "${CTEST_SCRIPT_DIRECTORY}/${CTEST_SCRIPT_NAME}")

#CTEST_EMPTY_BINARY_DIRECTORY(${CTEST_BINARY_DIRECTORY})

CTEST_START(Experimental)
CTEST_CONFIGURE(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
CTEST_BUILD(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
CTEST_TEST(BUILD "${CTEST_BINARY_DIRECTORY}" RETURN_VALUE res)
#CTEST_SUBMIT()
