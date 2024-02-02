##
# Download findFFTW script
# from: https://github.com/egpbos/findFFTW
##
cmake_minimum_required(VERSION 3.15)

if(USE_STATIC_LIBS)
  set(FFTW_USE_STATIC_LIBS true) # Request static flavor
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/downloadFindFFTW.cmake.in
${CMAKE_CURRENT_BINARY_DIR}/cmake/findFFTW-download/CMakeLists.txt
)

execute_process(
  COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
  RESULT_VARIABLE ret
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cmake/findFFTW-download
)

if(ret)
  message(FATAL_ERROR "CMake step for findFFTW failed: ${result}")
else()
  message("CMake step for findFFTW completed (${result}).")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cmake/findFFTW-download
)

if(result)
  message(FATAL_ERROR "Build step for findFFTW failed: ${result}")
endif()

set(findFFTW_DIR ${CMAKE_CURRENT_BINARY_DIR}/cmake/findFFTW-src)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${findFFTW_DIR}")