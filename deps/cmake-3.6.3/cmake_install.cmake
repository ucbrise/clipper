# Install script for directory: /Users/Chester/Cal/2/clipper/deps/cmake-3.6.3

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/Users/Chester/Cal/2/clipper/deps/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/doc/cmake-3.6" TYPE FILE FILES "/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Copyright.txt")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cmake-3.6" TYPE DIRECTORY PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ DIR_PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE FILES
    "/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Help"
    "/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Modules"
    "/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Templates"
    REGEX "/[^/]*\\.sh[^/]*$" PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Source/kwsys/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/KWIML/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmzlib/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmcurl/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmcompress/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmbzip2/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmliblzma/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmlibarchive/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmexpat/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmjsoncpp/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Source/CursesDialog/form/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Source/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Utilities/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Tests/cmake_install.cmake")
  include("/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/Auxiliary/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/Users/Chester/Cal/2/clipper/deps/cmake-3.6.3/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
