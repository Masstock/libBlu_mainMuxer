cmake_minimum_required(VERSION 3.15)

project(libBlu_mainMuxer
  VERSION 0.5
  DESCRIPTION "libBlu Muxer"
  HOMEPAGE_URL https://github.com/Masstock/libBlu_mainMuxer
  LANGUAGES C
)

set(APP_NAME mainMuxer)

# Build mode configuration
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

# see https://stackoverflow.com/a/64294837
get_property(isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)

if(NOT isMultiConfig)
  set(allowedBuildTypes Debug Release RelWithDebInfo MinSizeRel)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${allowedBuildTypes}")

  if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE IN_LIST allowedBuildTypes)
    message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
  endif()
endif()

message("Building mode: " ${CMAKE_BUILD_TYPE})

# Definitions
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_FSANITIZE "Enable fsanitize")
option(ENABLE_PROFILER  "Enable profiling compile options")
option(USE_STATIC_LIBS  "Request use of static libraries dependencies if possible")

set(gcc_like_c $<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>)
set(msvc_c $<COMPILE_LANG_AND_ID:CXX,MSVC>)

configure_file(
  "${PROJECT_SOURCE_DIR}/src/util/config.h.in"
  "${PROJECT_BINARY_DIR}/util/config.h"
)

configure_file(
  "${PROJECT_SOURCE_DIR}/settings.ini"
  "${PROJECT_BINARY_DIR}/settings.ini"
  COPYONLY
)

# Required packages
list(APPEND CMAKE_FIND_LIBRARY_SUFFIXES ".a")
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindFLEX.cmake) # Remove 'fl' library dependency

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(LibXml2 REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBASS REQUIRED libass>=0.15.0 IMPORTED_TARGET)
pkg_search_module(FFTW REQUIRED fftw3 IMPORTED_TARGET)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ini/)

flex_target(IniLexer
  src/ini/iniLexer.lex
  ${CMAKE_CURRENT_BINARY_DIR}/ini/iniLexer.yy.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/ini/iniLexer.yy.h
)

bison_target(IniParser
  src/ini/iniParser.y
  ${CMAKE_CURRENT_BINARY_DIR}/ini/iniParser.tab.c
  DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/ini/iniParser.tab.h
)

add_flex_bison_dependency(IniLexer
  IniParser
)

file(
  GLOB_RECURSE
  SRC_FILES
  ${SRC_DIR}/*
)

add_executable(${APP_NAME}
  ${SRC_FILES}
  ${FLEX_IniLexer_OUTPUTS}
  ${BISON_IniParser_OUTPUTS}
)

target_compile_options(${APP_NAME}
  PUBLIC "-Wall;-Wextra;-Winline;-Werror;-Wpointer-arith"
)

if(ENABLE_PROFILER)
  message("Profiling is enabled")
  target_compile_options(${APP_NAME} PUBLIC "-pg;-fprofile-arcs")
endif()

if(ENABLE_FSANITIZE)
  message("FSanitize is enabled")
  target_compile_options(${APP_NAME} PUBLIC "-fsanitize=address;-fsanitize=leak;-fno-omit-frame-pointer;-g;-static-libasan")
  target_link_options(${APP_NAME} PUBLIC "-fsanitize=address")
endif()

if(WIN32)
  set(SYSTEM_LIBRARIES libiconv)
else()
  set(SYSTEM_LIBRARIES dl)
endif()

target_compile_options(${APP_NAME}
  PUBLIC "$<$<CONFIG:Debug>:-g;-ggdb;-O0>$<$<CONFIG:Release>:-O2>"
)

target_include_directories(${APP_NAME} PRIVATE
  ${SRC_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBXML2_INCLUDE_DIRS}
  ${FFTW_INCLUDE_DIRS}
  PkgConfig::LIBASS
  PkgConfig::FFTW
)

target_link_libraries(${APP_NAME}
  ${SYSTEM_LIBRARIES}
#   ${FLEX_LIBRARIES}
  ${LIBXML2_LIBRARIES}
  ${FFTW_LIBRARIES}
  PkgConfig::LIBASS
  PkgConfig::FFTW
  m
)

target_compile_definitions(${APP_NAME} PRIVATE
  LIBBLU_CMAKE_USED
)
