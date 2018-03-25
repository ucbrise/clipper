# CMake generated Testfile for 
# Source directory: /Users/Jay/clipper/deps/cmake-3.6.3
# Build directory: /Users/Jay/clipper/deps/cmake-3.6.3
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
include("/Users/Jay/clipper/deps/cmake-3.6.3/Tests/EnforceConfig.cmake")
add_test(SystemInformationNew "/Users/Jay/clipper/deps/cmake-3.6.3/bin/cmake" "--system-information" "-G" "Unix Makefiles")
subdirs(Source/kwsys)
subdirs(Utilities/KWIML)
subdirs(Utilities/cmzlib)
subdirs(Utilities/cmcurl)
subdirs(Utilities/cmcompress)
subdirs(Utilities/cmbzip2)
subdirs(Utilities/cmliblzma)
subdirs(Utilities/cmlibarchive)
subdirs(Utilities/cmexpat)
subdirs(Utilities/cmjsoncpp)
subdirs(Source/CursesDialog/form)
subdirs(Source)
subdirs(Utilities)
subdirs(Tests)
subdirs(Auxiliary)
