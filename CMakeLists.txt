cmake_minimum_required(VERSION 3.15)

project(libBlu_mainMuxer
  VERSION 0.5
  DESCRIPTION "libBlu Muxer"
  HOMEPAGE_URL https://github.com/Masstock/libBlu_mainMuxer
  LANGUAGES C
)

set(APP_NAME mainMuxer)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()

message("Building mode: " ${CMAKE_BUILD_TYPE})

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_FSANITIZE "Enable fsanitize")
option(ENABLE_PROFILER  "Enable profiling compile options")

set(gcc_like_c $<COMPILE_LANG_AND_ID:CXX,ARMClang,AppleClang,Clang,GNU,LCC>)
set(msvc_c $<COMPILE_LANG_AND_ID:CXX,MSVC>)

configure_file(
  "${PROJECT_SOURCE_DIR}/src/util/config.h.in"
  "${PROJECT_BINARY_DIR}/util/config.h"
)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBXML REQUIRED libxml-2.0 IMPORTED_TARGET)
pkg_search_module(FFTW REQUIRED fftw3>=3.3.10 IMPORTED_TARGET)
pkg_search_module(LIBASS REQUIRED libass>=0.15.0 IMPORTED_TARGET)

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

set(GCC_COMPILE_OPTIONS "-Wall;-Wextra;-Winline;-Werror;-Wpointer-arith")

if(ENABLE_PROFILER)
  set(GCC_COMPILE_OPTIONS "${GCC_COMPILE_OPTIONS};-pg;-fprofile-arcs")
endif()

if(ENABLE_FSANITIZE)
  set(GCC_COMPILE_OPTIONS "${GCC_COMPILE_OPTIONS};-fsanitize=address;-fsanitize=leak")
endif()

if(WIN32)
  set(SYSTEM_LIBRARIES libiconv)
else()
  set(SYSTEM_LIBRARIES dl)
endif()

target_compile_options(${APP_NAME} PUBLIC
  "$<$<CONFIG:Debug>:${GCC_COMPILE_OPTIONS};-g;-ggdb;-O0>"
)
target_compile_options(${APP_NAME} PUBLIC
  "$<$<CONFIG:Release>:${GCC_COMPILE_OPTIONS};-O2>"
)

target_include_directories(${APP_NAME} PRIVATE
  ${SRC_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  PkgConfig::LIBXML
  PkgConfig::FFTW
  PkgConfig::LIBASS
)

target_link_libraries(${APP_NAME}
  ${SYSTEM_LIBRARIES}
  ${FLEX_LIBRARIES}
  PkgConfig::LIBXML
  PkgConfig::FFTW
  PkgConfig::LIBASS
  m
)

target_compile_definitions(${APP_NAME} PRIVATE
  LIBBLU_CMAKE_USED
)

# target_compile_options(${APP_NAME} INTERFACE
#   "$<${gcc_like_c}:-Wall;-Wextra;-Wshadow;-Wformat=2;-Wunused>"
#   "$<${msvc_c}:-W3>"
# )

# set(CMAKE_CXX_STANDARD 11)
# set(CMAKE_BUILD_TYPE "Release")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")

# find_package(DGtal REQUIRED)
# include_directories(${DGTAL_INCLUDE_DIRS})
# link_directories(${DGTAL_LIBRARY_DIRS})

# add_executable(gtia_tp2 main.cpp)
# target_link_libraries(gtia_tp2 ${DGTAL_LIBRARIES})
