# CMake generated Testfile for 
# Source directory: /Users/Jay/clipper/deps/cmake-3.6.3/Tests/CMakeLib
# Build directory: /Users/Jay/clipper/deps/cmake-3.6.3/Tests/CMakeLib
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(CMakeLib.testGeneratedFileStream "CMakeLibTests" "testGeneratedFileStream")
add_test(CMakeLib.testRST "CMakeLibTests" "testRST" "/Users/Jay/clipper/deps/cmake-3.6.3/Tests/CMakeLib")
add_test(CMakeLib.testSystemTools "CMakeLibTests" "testSystemTools")
add_test(CMakeLib.testUTF8 "CMakeLibTests" "testUTF8")
add_test(CMakeLib.testXMLParser "CMakeLibTests" "testXMLParser")
add_test(CMakeLib.testXMLSafe "CMakeLibTests" "testXMLSafe")
subdirs(PseudoMemcheck)
