set(url "file:///Users/Jay/clipper/deps/cmake-3.6.3/Tests/CMakeTests/FileDownloadInput.png")
set(dir "/Users/Jay/clipper/deps/cmake-3.6.3/Tests/CMakeTests/downloads")

file(DOWNLOAD
  ${url}
  ${dir}/file3.png
  TIMEOUT 2
  STATUS status
  EXPECTED_HASH SHA1=5555555555555555555555555555555555555555
  )
