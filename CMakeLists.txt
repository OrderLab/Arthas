# The Pmem-Meta Project
#
# Copyright (c) 2019, Johns Hopkins University - Order Lab.
#
#    All rights reserved.
#    Licensed under the Apache License, Version 2.0 (the "License");
#

cmake_minimum_required(VERSION 3.4.3)
project(pmem-meta)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED on)

# Must turn off rtti with -fno-rtti
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -g -std=c++11 -Wall -Wextra -Wno-unused-parameter")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(analyzer)

# Make clang-format into a make target to enforce coding style
# http://mariobadr.com/using-clang-format-to-enforce-style.html
find_program(
  CLANG_FORMAT_EXE
  NAMES "clang-format"
  DOC "Path to clang-format executable"
)
if(NOT CLANG_FORMAT_EXE)
  message(STATUS "clang-format not found.")
else()
  message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
  set(DO_CLANG_FORMAT "${CLANG_FORMAT_EXE}" "-i -style=file")
endif()

file(GLOB_RECURSE FILES_TO_FORMAT
  ${CMAKE_CURRENT_SOURCE_DIR}/analyzer/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/analyzer/*.h
)

if(CLANG_FORMAT_EXE)
  message(STATUS "running clang-format on all source files.")
  add_custom_target(
    format
    COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${FILES_TO_FORMAT}
  )
endif()
