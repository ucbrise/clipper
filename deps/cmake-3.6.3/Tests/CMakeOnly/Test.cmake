if (NOT TEST_SOURCE)
  set(TEST_SOURCE "${TEST}")
endif ()

set(make_program "/usr/bin/make")
if(make_program)
  set(maybe_make_program "-DCMAKE_MAKE_PROGRAM=${make_program}")
endif()

set(source_dir "/Users/Jay/clipper/deps/cmake-3.6.3/Tests/CMakeOnly/${TEST_SOURCE}")
set(binary_dir "/Users/Jay/clipper/deps/cmake-3.6.3/Tests/CMakeOnly/${TEST}-build")
file(REMOVE_RECURSE "${binary_dir}")
file(MAKE_DIRECTORY "${binary_dir}")
execute_process(
  COMMAND  ${CMAKE_COMMAND} ${CMAKE_ARGS}
  "${source_dir}" -G "Unix Makefiles"
  -A ""
  -T ""
  ${maybe_make_program}
  WORKING_DIRECTORY "${binary_dir}"
  RESULT_VARIABLE result
  )
if(result)
  message(FATAL_ERROR "CMake failed to configure ${TEST}")
endif()
